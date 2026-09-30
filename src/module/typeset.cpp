#include "typeset.h"

#include "font/fontname.h"

#include <algorithm>
#include <span>

namespace {

constexpr float kGap = 250;
constexpr double kEm = 1000;
constexpr double kLeading = 1.2;
constexpr double kAscent = 0.8;
constexpr double kDescent = 0.2;
constexpr char32_t kSpace = U' ';
constexpr char32_t kHyphen = U'-';

// Faces and size per style.
struct Style
{
    std::vector<Face *> chain;
    double size;
};

// One shaped character.
struct Slot
{
    Face *face;
    int style;
    Glyph glyph;
    char32_t code;
    qsizetype unit = 0;
    float kerned = 0;
};

struct Shaper
{
    FontLibrary &fonts;
    std::vector<Style> styles;
    std::vector<int> map;

    std::vector<Slot> shape(const QString &text, qsizetype begin, qsizetype end);
    Slot slotFor(char32_t c, int style, const std::vector<Slot> &before);
    double width(const Slot &slot) const { return slot.kerned * styles[size_t(slot.style)].size / kEm; }
    double width(std::span<const Slot> stretch) const;
};

std::vector<Face *> chainFor(const FontChoice &choice, const QString &text, FontLibrary &fonts)
{
    std::vector<Face *> chain;
    if (choice.pageFont)
        chain = fonts.pageFaces(choice.pageFont);
    else if (Face *face = fonts.fileFace({choice.family, QStringLiteral("sans-serif"), choice.bold, choice.italic, {}}))
        chain.push_back(face);
    const Face *lead = chain.empty() ? nullptr : chain.front();
    const QString family = lead ? familyOf(lead->name) : choice.family;
    const QString generic = lead && lead->serif ? QStringLiteral("serif") : QStringLiteral("sans-serif");
    const bool bold = lead ? lead->bold : choice.bold;
    const bool italic = lead ? lead->italic : choice.italic;
    Face *fallback = fonts.fileFace({family, generic, bold, italic, text});
    if (fallback && std::find(chain.begin(), chain.end(), fallback) == chain.end())
        chain.push_back(fallback);
    return chain;
}

// Body then spans, shared fonts.
std::vector<Style> stylesOf(const Module &module, FontLibrary &fonts)
{
    std::vector<Style> styles{{chainFor(module.font, module.text, fonts), module.size}};
    std::vector<const FontChoice *> resolved{&module.font};
    for (const Span &span : module.spans) {
        const auto same = std::ranges::find_if(resolved, [&span](const FontChoice *c) { return *c == span.font; });
        const bool known = same != resolved.end();
        std::vector<Face *> chain =
            known ? styles[size_t(same - resolved.begin())].chain : chainFor(span.font, module.text, fonts);
        resolved.push_back(&span.font);
        styles.push_back({std::move(chain), module.size * span.scale});
    }
    return styles;
}

// Style per unit, body zero.
std::vector<int> styleMap(const Module &module)
{
    std::vector<int> map(size_t(module.text.size()), 0);
    for (size_t i = 0; i < module.spans.size(); ++i)
        std::fill_n(map.begin() + module.spans[i].start, module.spans[i].length, int(i + 1));
    return map;
}

Slot Shaper::slotFor(char32_t c, int style, const std::vector<Slot> &before)
{
    const std::vector<Face *> &chain = styles[size_t(style)].chain;
    for (Face *face : chain) {
        if (const std::optional<Glyph> found = fonts.glyph(*face, c))
            return {face, style, *found, c};
    }
    const bool follow = !before.empty() && before.back().style == style;
    Face *owner = follow ? before.back().face : (chain.empty() ? nullptr : chain.front());
    return {owner, style, {0, -1, kGap}, c};
}

bool sameStretch(const Slot &a, const Slot &b)
{
    return a.face == b.face && a.style == b.style;
}

// Kerned advances within one face.
void kernStretch(std::span<Slot> stretch)
{
    const Face *face = stretch.front().face;
    const bool fileFace = face && !face->object;
    QList<quint32> gids;
    for (const Slot &slot : stretch)
        gids.append(slot.glyph.gid);
    const QList<QPointF> advances =
        fileFace ? face->raw.advancesForGlyphIndexes(gids, QRawFont::KernedAdvances) : QList<QPointF>();
    for (size_t i = 0; i < stretch.size(); ++i) {
        const bool drawn = fileFace && stretch[i].glyph.gid;
        stretch[i].kerned = drawn ? float(advances[qsizetype(i)].x()) : stretch[i].glyph.advance;
    }
}

std::vector<Slot> Shaper::shape(const QString &text, qsizetype begin, qsizetype end)
{
    std::vector<Slot> shaped;
    qsizetype unit = begin;
    for (char32_t c : QStringView(text).sliced(begin, end - begin).toUcs4()) {
        shaped.push_back(slotFor(c, map[size_t(unit)], shaped));
        shaped.back().unit = unit;
        unit += QChar::requiresSurrogates(c) ? 2 : 1;
    }
    for (size_t first = 0, last = 0; first < shaped.size(); first = last) {
        last = first + 1;
        while (last < shaped.size() && sameStretch(shaped[first], shaped[last]))
            ++last;
        kernStretch(std::span(shaped).subspan(first, last - first));
    }
    return shaped;
}

double Shaper::width(std::span<const Slot> stretch) const
{
    double sum = 0;
    for (const Slot &slot : stretch)
        sum += width(slot);
    return sum;
}

// Hyphen inside a word.
bool breaksAfter(std::span<const Slot> shaped, size_t i)
{
    return i > 0 && shaped[i].code == kHyphen && QChar::isLetterOrNumber(shaped[i - 1].code);
}

// Spaces, or word to hyphen.
size_t chunkEnd(std::span<const Slot> shaped, size_t first)
{
    const bool space = shaped[first].code == kSpace;
    size_t end = first + 1;
    while (end < shaped.size() && (shaped[end].code == kSpace) == space && !breaksAfter(shaped, end - 1))
        ++end;
    return end;
}

// Greedy breaks at spaces, hyphens.
std::vector<std::span<const Slot>> wrap(const Shaper &shaper, std::span<const Slot> shaped, double limit)
{
    std::vector<std::span<const Slot>> lines;
    size_t start = 0;
    double used = 0;
    bool worded = false;
    for (size_t i = 0, end = 0; i < shaped.size(); i = end) {
        end = chunkEnd(shaped, i);
        const double width = shaper.width(shaped.subspan(i, end - i));
        const bool word = shaped[i].code != kSpace;
        if (word && worded && limit > 0 && used + width > limit) {
            const size_t cut = shaped[i - 1].code == kSpace ? i - 1 : i;
            lines.push_back(shaped.subspan(start, cut - start));
            start = i;
            used = 0;
        }
        used += width;
        worded = worded || word;
    }
    lines.push_back(shaped.subspan(start));
    return lines;
}

GlyphRun runFrom(const Shaper &shaper, const Slot &slot, QPointF origin)
{
    const double size = shaper.styles[size_t(slot.style)].size;
    GlyphRun run{slot.face, slot.face->raw, origin, size, slot.style, {}, {}, {}};
    run.sized.setPixelSize(size);
    return run;
}

void place(const Shaper &shaper, std::span<const Slot> line, double y, Layout &layout)
{
    const int row = int(layout.baselines.size());
    layout.baselines.push_back(y);
    double x = 0;
    for (size_t i = 0; i < line.size(); ++i) {
        const Slot &slot = line[i];
        layout.stops[size_t(slot.unit)] = {x, row};
        if (QChar::requiresSurrogates(slot.code))
            layout.stops[size_t(slot.unit) + 1] = {x, row};
        if (!slot.face)
            continue;
        if (i == 0 || !sameStretch(line[i - 1], slot))
            layout.runs.push_back(runFrom(shaper, slot, QPointF(x, y)));
        GlyphRun &run = layout.runs.back();
        const quint32 gid = slot.glyph.gid;
        if (gid) {
            run.gids.append(gid);
            run.positions.append(QPointF(x - run.origin.x(), 0));
        }
        run.items.push_back({gid ? slot.glyph.code : -1, gid ? slot.glyph.advance - slot.kerned : -slot.kerned});
        x += shaper.width(slot);
    }
    layout.bounds.setRight(std::max(layout.bounds.right(), x));
    if (line.empty())
        return;
    const Slot &last = line.back();
    layout.stops[size_t(last.unit + (QChar::requiresSurrogates(last.code) ? 2 : 1))] = {x, row};
}

} // namespace

Layout typeset(const Module &module, FontLibrary &fonts)
{
    Shaper shaper{fonts, stylesOf(module, fonts), styleMap(module)};
    const double size = module.size;
    const double leading = module.leading > 0 ? module.leading : size * kLeading;
    const std::vector<Face *> &body = shaper.styles.front().chain;
    const QRawFont *metrics = body.empty() ? nullptr : &body.front()->raw;
    const double ascent = metrics ? metrics->ascent() * size / kEm : size * kAscent;
    const double descent = metrics ? metrics->descent() * size / kEm : size * kDescent;
    Layout layout{{}, {}, std::vector<Stop>(size_t(module.text.size()) + 1, {0, 0}), {}, ascent, descent};
    int line = 0;
    for (qsizetype begin = 0, end = 0; begin <= module.text.size(); begin = end + 1) {
        end = module.text.indexOf('\n', begin);
        end = end < 0 ? module.text.size() : end;
        const std::vector<Slot> shaped = shaper.shape(module.text, begin, end);
        for (std::span<const Slot> text : wrap(shaper, shaped, module.width))
            place(shaper, text, line++ * leading, layout);
        if (shaped.empty())
            layout.stops[size_t(begin)] = {0, line - 1};
    }
    layout.bounds.setTop(-ascent);
    layout.bounds.setBottom((line - 1) * leading + descent);
    layout.bounds.setRight(std::max({layout.bounds.right(), module.width, size}));
    return layout;
}
