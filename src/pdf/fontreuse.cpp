#include "fontreuse.h"

#include "fontname.h"
#include "systemfont.h"

#include <QHash>
#include <algorithm>
#include <cstring>

namespace {

constexpr float kWordGap = 250;
constexpr int kFormDepth = 4;
constexpr char32_t kSpace = U' ';
constexpr size_t kNone = size_t(-1);

// One font able to draw glyphs.
struct Source
{
    pdf_obj *font = nullptr;
    pdf_font_desc *desc = nullptr;
    fz_font *program = nullptr;
    int bytes = 2;
    QHash<char32_t, int> codes;
};

int codeWidth(const pdf_cmap *cmap)
{
    const int bytes = cmap->codespace_len > 0 ? cmap->codespace[0].n : 0;
    for (int i = 1; i < cmap->codespace_len; ++i) {
        if (cmap->codespace[i].n != bytes)
            return 0;
    }
    return bytes == 1 || bytes == 2 ? bytes : 0;
}

pdf_font_desc *tryLoad(fz_context *ctx, pdf_document *doc, pdf_obj *resources, pdf_obj *dict)
{
    pdf_resource_stack stack{nullptr, resources};
    pdf_font_desc *desc = nullptr;
    fz_var(desc);
    fz_try(ctx)
        desc = pdf_load_font(ctx, doc, &stack, dict);
    fz_catch(ctx)
        fz_ignore_error(ctx);
    return desc;
}

bool known(fz_context *ctx, const std::vector<Source> &sources, pdf_obj *dict)
{
    return std::any_of(sources.begin(), sources.end(),
                       [&](const Source &s) { return pdf_resolve_indirect(ctx, s.font) == pdf_resolve_indirect(ctx, dict); });
}

// Page fonts sharing the run's typeface.
void collect(fz_context *ctx, pdf_document *doc, pdf_obj *resources, const char *base, int depth,
             std::vector<Source> &sources)
{
    pdf_obj *fonts = pdf_dict_get(ctx, resources, PDF_NAME(Font));
    for (int i = 0; i < pdf_dict_len(ctx, fonts); ++i) {
        pdf_obj *dict = pdf_dict_get_val(ctx, fonts, i);
        pdf_font_desc *desc = known(ctx, sources, dict) ? nullptr : tryLoad(ctx, doc, resources, dict);
        const bool usable = desc && !desc->wmode && codeWidth(desc->encoding) &&
                            std::strcmp(baseFontName(fz_font_name(ctx, desc->font)), base) == 0;
        if (usable)
            sources.push_back({pdf_keep_obj(ctx, dict), desc, nullptr, codeWidth(desc->encoding), {}});
        else
            pdf_drop_font(ctx, desc);
    }
    pdf_obj *xobjects = pdf_dict_get(ctx, resources, PDF_NAME(XObject));
    for (int i = 0; depth > 0 && i < pdf_dict_len(ctx, xobjects); ++i) {
        pdf_obj *form = pdf_dict_get_val(ctx, xobjects, i);
        if (pdf_name_eq(ctx, pdf_dict_get(ctx, form, PDF_NAME(Subtype)), PDF_NAME(Form)))
            collect(ctx, doc, pdf_dict_get(ctx, form, PDF_NAME(Resources)), base, depth - 1, sources);
    }
}

char32_t unicodeOf(pdf_font_desc *desc, int cid)
{
    int buffer[PDF_MRANGE_CAP];
    if (desc->to_unicode && pdf_lookup_cmap_full(desc->to_unicode, unsigned(cid), buffer) == 1)
        return char32_t(buffer[0]);
    if (size_t(cid) < desc->cid_to_ucs_len)
        return desc->cid_to_ucs[cid];
    return 0;
}

bool inked(fz_context *ctx, fz_font *font, int gid)
{
    fz_rect box = fz_empty_rect;
    fz_var(box);
    fz_try(ctx)
        box = fz_bound_glyph(ctx, font, gid, fz_identity);
    fz_catch(ctx)
        fz_ignore_error(ctx);
    return !fz_is_empty_rect(box);
}

int glyphOf(const pdf_font_desc *desc, int cid)
{
    if (!desc->cid_to_gid)
        return cid;
    return size_t(cid) < desc->cid_to_gid_len ? desc->cid_to_gid[cid] : 0;
}

// Drawable code per wanted character.
void mapCodes(fz_context *ctx, Source &source, const std::u32string &wanted)
{
    const int limit = 1 << (8 * source.bytes);
    for (int code = 0; code < limit && size_t(source.codes.size()) < wanted.size(); ++code) {
        const int cid = pdf_lookup_cmap(source.desc->encoding, unsigned(code));
        const char32_t ucs = cid < 0 ? 0 : unicodeOf(source.desc, cid);
        if (!ucs || source.codes.contains(ucs) || wanted.find(ucs) == std::u32string::npos)
            continue;
        const int gid = glyphOf(source.desc, cid);
        if (gid > 0 && (ucs == kSpace || inked(ctx, source.desc->font, gid)))
            source.codes.insert(ucs, code);
    }
}

int codeFor(fz_context *ctx, const Source &source, char32_t c)
{
    if (source.desc)
        return source.codes.value(c, -1);
    const int gid = fz_encode_character(ctx, source.program, int(c));
    return gid > 0 ? gid : -1;
}

size_t pick(fz_context *ctx, const std::vector<Source> &sources, char32_t c)
{
    for (size_t i = 0; i < sources.size(); ++i) {
        if (codeFor(ctx, sources[i], c) >= 0)
            return i;
    }
    return kNone;
}

float widthOf(fz_context *ctx, const Source &source, int code)
{
    if (source.desc)
        return float(pdf_lookup_hmtx(ctx, source.desc, pdf_lookup_cmap(source.desc->encoding, unsigned(code))).w);
    return fz_advance_glyph(ctx, source.program, code, 0) * 1000;
}

fz_font *openFont(fz_context *ctx, const FontFile &file)
{
    fz_font *font = nullptr;
    fz_var(font);
    fz_try(ctx)
        font = fz_new_font_from_file(ctx, nullptr, file.path.constData(), file.index, 0);
    fz_catch(ctx)
        fz_ignore_error(ctx);
    return font;
}

pdf_obj *embed(fz_context *ctx, pdf_document *doc, fz_font *font)
{
    pdf_obj *added = nullptr;
    fz_var(added);
    fz_try(ctx)
        added = pdf_add_cid_font(ctx, doc, font);
    fz_catch(ctx)
        fz_ignore_error(ctx);
    return added;
}

// Metric-compatible installed font for the run.
QString addSystemFont(fz_context *ctx, pdf_document *doc, fz_font *original, const QString &text,
                      std::vector<Source> &sources)
{
    const QString name = QString::fromUtf8(fz_font_name(ctx, original));
    const QString generic = fz_font_is_monospaced(ctx, original) ? QStringLiteral("monospace")
                            : fz_font_is_serif(ctx, original)    ? QStringLiteral("serif")
                                                                 : QStringLiteral("sans-serif");
    const FontFile file = matchFont({familyOf(name), generic, fz_font_is_bold(ctx, original) || namesBold(name),
                                     bool(fz_font_is_italic(ctx, original)), text});
    fz_font *font = file.path.isEmpty() ? nullptr : openFont(ctx, file);
    pdf_obj *added = font ? embed(ctx, doc, font) : nullptr;
    if (!added) {
        fz_drop_font(ctx, font);
        return {};
    }
    sources.push_back({added, nullptr, font, 2, {}});
    return file.family;
}

void release(fz_context *ctx, std::vector<Source> &sources)
{
    for (Source &source : sources) {
        pdf_drop_obj(ctx, source.font);
        pdf_drop_font(ctx, source.desc);
        fz_drop_font(ctx, source.program);
    }
}

} // namespace

QString encodeText(fz_context *ctx, pdf_page *page, fz_font *font, const QString &text, Encoding *out)
{
    pdf_document *doc = page->doc;
    const std::u32string wanted = text.toStdU32String();
    std::u32string distinct;
    for (char32_t c : wanted) {
        if (distinct.find(c) == std::u32string::npos)
            distinct += c;
    }
    std::vector<Source> sources;
    collect(ctx, doc, pdf_page_resources(ctx, page), baseFontName(fz_font_name(ctx, font)), kFormDepth, sources);
    std::stable_partition(sources.begin(), sources.end(), [font](const Source &s) { return s.desc->font == font; });
    for (Source &source : sources)
        mapCodes(ctx, source, distinct);

    std::vector<size_t> owners;
    for (char32_t c : wanted) {
        size_t index = pick(ctx, sources, c);
        if (index == kNone && c != kSpace && !out->embedded) {
            out->substitute = addSystemFont(ctx, doc, font, text, sources);
            out->embedded = !out->substitute.isEmpty();
            index = pick(ctx, sources, c);
        }
        if (index == kNone && c != kSpace) {
            release(ctx, sources);
            return QStringLiteral("No installed font can draw “%1”").arg(QString::fromUcs4(&c, 1));
        }
        if (index == kNone && owners.empty())
            continue;
        const int code = index == kNone ? -1 : codeFor(ctx, sources[index], c);
        index = index == kNone ? owners.back() : index;
        if (owners.empty() || owners.back() != index) {
            out->segments.push_back({pdf_keep_obj(ctx, sources[index].font), sources[index].bytes, {}, 0});
            owners.push_back(index);
        }
        Segment &segment = out->segments.back();
        segment.items.push_back({code, code < 0 ? -kWordGap : 0});
        segment.advance += code < 0 ? kWordGap : widthOf(ctx, sources[index], code);
    }
    release(ctx, sources);
    return {};
}

void dropEncoding(fz_context *ctx, Encoding &encoding)
{
    for (Segment &segment : encoding.segments)
        pdf_drop_obj(ctx, segment.font);
    encoding.segments.clear();
}
