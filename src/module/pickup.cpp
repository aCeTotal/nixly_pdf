#include "pickup.h"

#include "pdf/content.h"
#include "pdf/document.h"
#include "font/fontname.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr double kSlack = 1.03;
constexpr double kSizeTolerance = 0.05;

struct Job
{
    const Lift &lift;
    FontLibrary &fonts;
    std::vector<int> objects;
    std::vector<std::pair<fz_font *, int>> adopted;
};

// Run font, size and ink.
struct Look
{
    FontChoice font;
    double size;
    QRgb color;
};

// Distinct looks, one per run.
struct Palette
{
    std::vector<Look> looks;
    std::vector<size_t> ofRun;
    size_t body = 0;
};

const LiveRun *findLive(const std::vector<LiveRun> &live, const TextRun &run)
{
    const auto hit = std::ranges::find_if(live, [&run](const LiveRun &l) { return l.run.glyphs == run.glyphs; });
    return hit == live.end() ? nullptr : &*hit;
}

// Adopts each page font once.
int adoptOnce(fz_context *ctx, pdf_page *page, fz_font *font, Job &job)
{
    const auto seen = std::ranges::find(job.adopted, font, &std::pair<fz_font *, int>::first);
    if (seen != job.adopted.end())
        return seen->second;
    const int object = job.fonts.adopt(ctx, page, font);
    job.adopted.emplace_back(font, object);
    return object;
}

QString strip(fz_context *ctx, pdf_page *page, fz_stext_page *text, Job &job)
{
    const std::vector<LiveRun> live = collectRuns(ctx, text);
    std::vector<fz_point> removed;
    fz_matrix ctm;
    pdf_page_transform(ctx, page, nullptr, &ctm);
    const fz_matrix toUser = fz_invert_matrix(ctm);
    for (const TextRun &run : job.lift.passage.runs) {
        const LiveRun *match = findLive(live, run);
        if (!match)
            return QStringLiteral("The text moved; try again");
        job.objects.push_back(adoptOnce(ctx, page, match->font, job));
        for (const QPointF &p : run.glyphs)
            removed.push_back(fz_transform_point(fz_make_point(float(p.x()), float(p.y())), toUser));
    }
    return rewritePage(ctx, page, {removed, {}, {}, {}});
}

QString lift(fz_context *ctx, pdf_document *doc, Job &job)
{
    pdf_page *page = nullptr;
    fz_stext_page *text = nullptr;
    QString failure = attempt(ctx, [&] {
        page = pdf_load_page(ctx, doc, job.lift.index);
        text = extractText(ctx, &page->super);
    });
    if (failure.isEmpty())
        failure = strip(ctx, page, text, job);
    fz_drop_stext_page(ctx, text);
    pdf_drop_page(ctx, page);
    return failure;
}

bool sameLook(const Look &a, const Look &b)
{
    return a.font == b.font && std::fabs(a.size - b.size) < kSizeTolerance && a.color == b.color;
}

// Body is the commonest look.
Palette paletteOf(const std::vector<TextRun> &runs, const std::vector<int> &objects)
{
    Palette palette;
    std::vector<qsizetype> weight;
    for (size_t i = 0; i < runs.size(); ++i) {
        const TextRun &run = runs[i];
        const Look look{{objects[i], objects[i] ? QString() : familyOf(run.font), run.bold, run.italic},
                        run.size,
                        run.color};
        const auto seen = std::ranges::find_if(palette.looks, [&look](const Look &l) { return sameLook(l, look); });
        const size_t kind = size_t(seen - palette.looks.begin());
        if (seen == palette.looks.end()) {
            palette.looks.push_back(look);
            weight.push_back(0);
        }
        palette.ofRun.push_back(kind);
        weight[kind] += run.text.size();
    }
    palette.body = size_t(std::ranges::max_element(weight) - weight.begin());
    return palette;
}

Span spanOf(const Look &look, const Look &body, int start)
{
    const QColor ink = look.color == body.color ? QColor() : QColor::fromRgb(look.color);
    return {start, 0, look.font, look.size / body.size, ink};
}

// Runs into text and spans.
void compose(Module &module, const Passage &passage, const Palette &palette)
{
    const Look &body = palette.looks[palette.body];
    size_t open = palette.body;
    const auto write = [&](const QString &piece, size_t kind) {
        const int start = int(module.text.size());
        module.text += piece;
        if (kind == palette.body || piece.isEmpty())
            return;
        const bool joined = kind == open && !module.spans.empty() &&
                            module.spans.back().start + module.spans.back().length == start;
        if (!joined)
            module.spans.push_back(spanOf(palette.looks[kind], body, start));
        module.spans.back().length += int(piece.size());
        open = kind;
    };
    for (size_t i = 0; i < passage.runs.size(); ++i) {
        const size_t kind = palette.ofRun[i];
        write(passage.joints[i], i > 0 && palette.ofRun[i - 1] == kind ? kind : palette.body);
        write(passage.runs[i].text, kind);
    }
}

Module moduleFrom(const Passage &passage, int page, const std::vector<int> &objects)
{
    const TextRun &first = passage.runs.front();
    const Palette palette = paletteOf(passage.runs, objects);
    const Look &body = palette.looks[palette.body];
    Module module;
    module.page = page;
    module.anchor = first.glyphs.front();
    module.angle = std::atan2(first.direction.y(), first.direction.x());
    module.leading = passage.leading;
    if (passage.leading > 0) {
        const QRectF widest = std::ranges::max(passage.outlines, {}, &QRectF::right);
        module.width = (widest.right() - module.anchor.x()) * kSlack;
    }
    module.font = body.font;
    module.originalFont = body.font.pageFont;
    module.size = body.size;
    module.color = QColor::fromRgb(body.color);
    compose(module, passage, palette);
    return module;
}

} // namespace

Lifted pickUp(Document &doc, FontLibrary &fonts, const Lift &lift)
{
    Job job{lift, fonts, {}, {}};
    Lifted lifted;
    doc.modify(&lifted.error, [&](fz_context *ctx, pdf_document *pdf) { return ::lift(ctx, pdf, job); });
    if (lifted.error.isEmpty())
        lifted.module = moduleFrom(lift.passage, doc.slot(lift.index).id, job.objects);
    return lifted;
}
