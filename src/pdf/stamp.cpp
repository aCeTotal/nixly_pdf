#include "stamp.h"

#include "document.h"

namespace {

pdf_obj *embedFile(fz_context *ctx, pdf_document *doc, const char *path)
{
    fz_font *font = fz_new_font_from_file(ctx, nullptr, path, 0, 0);
    pdf_obj *added = nullptr;
    fz_try(ctx)
        added = pdf_add_cid_font(ctx, doc, font);
    fz_always(ctx)
        fz_drop_font(ctx, font);
    fz_catch(ctx)
        fz_rethrow(ctx);
    return added;
}

fz_matrix lineMatrix(const StampLine &line, fz_matrix toUser)
{
    const fz_matrix page{line.size, 0, 0, -line.size, float(line.baseline.x()), float(line.baseline.y())};
    return fz_concat(page, toUser);
}

QString stamp(fz_context *ctx, pdf_document *doc, int index, const std::vector<StampLine> &lines)
{
    std::vector<TextSpan> spans(lines.size());
    pdf_page *page = nullptr;
    QString failure = attempt(ctx, [&] {
        page = pdf_load_page(ctx, doc, index);
        fz_matrix ctm;
        pdf_page_transform(ctx, page, nullptr, &ctm);
        const fz_matrix toUser = fz_invert_matrix(ctm);
        for (size_t i = 0; i < lines.size(); ++i) {
            spans[i].font = embedFile(ctx, doc, lines[i].fontPath.constData());
            spans[i].codeBytes = 2;
            spans[i].matrix = lineMatrix(lines[i], toUser);
            spans[i].color = lines[i].color;
        }
    });
    for (size_t i = 0; i < lines.size(); ++i)
        spans[i].items = lines[i].glyphs;
    if (failure.isEmpty())
        failure = rewritePage(ctx, doc, page, {}, spans);
    for (const TextSpan &span : spans)
        pdf_drop_obj(ctx, span.font);
    pdf_drop_page(ctx, page);
    return failure;
}

} // namespace

QString placeStamp(Document &doc, int index, const std::vector<StampLine> &lines)
{
    QString error;
    doc.modify(&error, [&](fz_context *ctx, pdf_document *pdf) { return stamp(ctx, pdf, index, lines); });
    if (error.isEmpty())
        doc.markFontEmbedded();
    return error;
}
