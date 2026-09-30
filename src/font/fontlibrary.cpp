#include "fontlibrary.h"

#include "fontname.h"

#include <QFile>
#include <QFontDatabase>
#include <cstring>

namespace {

constexpr qreal kDesignSize = 1000;
constexpr int kFormDepth = 4;
constexpr char32_t kSpace = U' ';

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

struct Found
{
    int object;
    pdf_font_desc *desc;
};

// Page fonts sharing one typeface.
void collect(fz_context *ctx, pdf_document *doc, pdf_obj *resources, const char *base, int depth,
             std::vector<Found> &found)
{
    pdf_obj *fonts = pdf_dict_get(ctx, resources, PDF_NAME(Font));
    for (int i = 0; i < pdf_dict_len(ctx, fonts); ++i) {
        pdf_obj *dict = pdf_dict_get_val(ctx, fonts, i);
        const int object = pdf_to_num(ctx, dict);
        const bool seen = std::ranges::any_of(found, [object](const Found &f) { return f.object == object; });
        pdf_font_desc *desc = object && !seen ? tryLoad(ctx, doc, resources, dict) : nullptr;
        const bool usable = desc && !desc->wmode && codeWidth(desc->encoding) && desc->font->buffer &&
                            std::strcmp(baseFontName(fz_font_name(ctx, desc->font)), base) == 0;
        if (usable)
            found.push_back({object, desc});
        else
            pdf_drop_font(ctx, desc);
    }
    pdf_obj *xobjects = pdf_dict_get(ctx, resources, PDF_NAME(XObject));
    for (int i = 0; depth > 0 && i < pdf_dict_len(ctx, xobjects); ++i) {
        pdf_obj *form = pdf_dict_get_val(ctx, xobjects, i);
        if (pdf_name_eq(ctx, pdf_dict_get(ctx, form, PDF_NAME(Subtype)), PDF_NAME(Form)))
            collect(ctx, doc, pdf_dict_get(ctx, form, PDF_NAME(Resources)), base, depth - 1, found);
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

// Unicode table of drawable codes.
void fillGlyphs(fz_context *ctx, pdf_font_desc *desc, Face &face)
{
    const int limit = 1 << (8 * face.codeBytes);
    for (int code = 0; code < limit; ++code) {
        const int cid = pdf_lookup_cmap(desc->encoding, unsigned(code));
        const char32_t ucs = cid < 0 ? 0 : unicodeOf(desc, cid);
        if (!ucs || face.glyphs.contains(ucs))
            continue;
        const int gid = pdf_font_cid_to_gid(ctx, desc, cid);
        if (gid > 0 && (ucs == kSpace || inked(ctx, desc->font, gid)))
            face.glyphs.insert(ucs, {quint32(gid), code, float(pdf_lookup_hmtx(ctx, desc, cid).w)});
    }
}

QString registerData(const QByteArray &data)
{
    return QFontDatabase::applicationFontFamilies(QFontDatabase::addApplicationFontFromData(data)).value(0);
}

std::unique_ptr<Face> pageFace(fz_context *ctx, const Found &found)
{
    fz_font *font = found.desc->font;
    unsigned char *bytes = nullptr;
    const size_t length = fz_buffer_storage(ctx, font->buffer, &bytes);
    const QByteArray data(reinterpret_cast<const char *>(bytes), qsizetype(length));
    auto face = std::make_unique<Face>();
    face->raw.loadFromData(data, kDesignSize, QFont::PreferNoHinting);
    if (!face->raw.isValid())
        return nullptr;
    face->family = registerData(data);
    face->name = QString::fromUtf8(fz_font_name(ctx, font));
    face->object = found.object;
    face->codeBytes = codeWidth(found.desc->encoding);
    face->bold = fz_font_is_bold(ctx, font) || namesBold(face->name);
    face->italic = fz_font_is_italic(ctx, font);
    face->serif = fz_font_is_serif(ctx, font);
    fillGlyphs(ctx, found.desc, *face);
    return face;
}

} // namespace

int FontLibrary::adopt(fz_context *ctx, pdf_page *page, fz_font *font)
{
    std::vector<Found> found;
    collect(ctx, page->doc, pdf_page_resources(ctx, page), baseFontName(fz_font_name(ctx, font)), kFormDepth, found);
    std::stable_partition(found.begin(), found.end(), [font](const Found &f) { return f.desc->font == font; });
    std::vector<int> objects;
    for (const Found &f : found) {
        if (!pages.contains(f.object)) {
            if (std::unique_ptr<Face> face = pageFace(ctx, f))
                pages.emplace(f.object, std::move(face));
        }
        if (pages.contains(f.object))
            objects.push_back(f.object);
        pdf_drop_font(ctx, f.desc);
    }
    if (objects.empty())
        return 0;
    siblings[objects.front()] = objects;
    return objects.front();
}

std::vector<Face *> FontLibrary::pageFaces(int object)
{
    std::vector<Face *> faces;
    for (int sibling : siblings[object])
        faces.push_back(pages.at(sibling).get());
    return faces;
}

Face *FontLibrary::fileFace(const FontQuery &query)
{
    const FontFile file = matchFont(query);
    return file.path.isEmpty() ? nullptr : fileAt(file.path);
}

Face *FontLibrary::fileAt(const QByteArray &path)
{
    auto hit = files.find(path);
    if (hit != files.end())
        return hit->second.get();
    auto face = std::make_unique<Face>();
    const QString file = QFile::decodeName(path);
    face->raw.loadFromFile(file, kDesignSize, QFont::PreferNoHinting);
    if (!face->raw.isValid())
        return nullptr;
    face->family = QFontDatabase::applicationFontFamilies(QFontDatabase::addApplicationFont(file)).value(0);
    face->name = face->raw.familyName();
    face->path = path;
    face->bold = face->raw.weight() >= QFont::DemiBold;
    face->italic = face->raw.style() != QFont::StyleNormal;
    return files.emplace(path, std::move(face)).first->second.get();
}

std::optional<Glyph> FontLibrary::glyph(Face &face, char32_t c)
{
    const auto hit = face.glyphs.constFind(c);
    if (hit != face.glyphs.constEnd())
        return hit->gid ? std::optional<Glyph>(*hit) : std::nullopt;
    if (face.object)
        return std::nullopt;
    const QList<quint32> gids = face.raw.glyphIndexesForString(QString::fromUcs4(&c, 1));
    const quint32 gid = gids.size() == 1 ? gids.front() : 0;
    const float advance = gid ? float(face.raw.advancesForGlyphIndexes({gid}).front().x()) : 0;
    face.glyphs.insert(c, {gid, int(gid), advance});
    return gid ? std::optional<Glyph>(face.glyphs.value(c)) : std::nullopt;
}
