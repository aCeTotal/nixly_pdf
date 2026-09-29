#include "textedit.h"

#include "content.h"
#include "document.h"
#include "fontreuse.h"

#include <algorithm>

namespace {

const LiveRun *findRun(const std::vector<LiveRun> &runs, const TextRun &target)
{
    const auto hit = std::find_if(runs.begin(), runs.end(), [&target](const LiveRun &live) {
        return live.run.glyphs == target.glyphs && live.run.size == target.size;
    });
    return hit == runs.end() ? nullptr : &*hit;
}

// Run glyph space to user.
fz_matrix runMatrix(const TextRun &run, fz_matrix toUser)
{
    const float s = run.size;
    const QPointF d = run.direction;
    const QPointF o = run.glyphs.front();
    const fz_matrix page{float(d.x()) * s, float(d.y()) * s, float(d.y()) * s, float(-d.x()) * s,
                         float(o.x()), float(o.y())};
    return fz_concat(page, toUser);
}

QString rewrite(fz_context *ctx, pdf_page *page, fz_stext_page *text, const TextRun &target,
                const QString &replacement, Encoding *encoding)
{
    const std::vector<LiveRun> runs = collectRuns(ctx, text);
    const LiveRun *live = findRun(runs, target);
    if (!live)
        return QStringLiteral("The text moved; try again");

    fz_matrix ctm;
    pdf_page_transform(ctx, page, nullptr, &ctm);
    const fz_matrix toUser = fz_invert_matrix(ctm);
    std::vector<fz_point> removed;
    for (const QPointF &p : live->run.glyphs)
        removed.push_back(fz_transform_point(fz_make_point(float(p.x()), float(p.y())), toUser));

    std::vector<TextSpan> spans;
    if (!replacement.isEmpty()) {
        const QString failure = encodeText(ctx, page, live->font, replacement, encoding);
        if (!failure.isEmpty())
            return failure;
        const fz_matrix base = runMatrix(live->run, toUser);
        float offset = 0;
        for (const Segment &segment : encoding->segments) {
            spans.push_back({segment.font, segment.codeBytes, fz_pre_translate(base, offset / 1000, 0),
                             live->run.color, segment.items});
            offset += segment.advance;
        }
    }
    return rewritePage(ctx, page->doc, page, removed, spans);
}

QString edit(fz_context *ctx, pdf_document *doc, int index, const TextRun &target, const QString &replacement,
             Encoding *encoding)
{
    pdf_page *page = nullptr;
    fz_stext_page *text = nullptr;
    QString failure = attempt(ctx, [&] {
        page = pdf_load_page(ctx, doc, index);
        text = extractText(ctx, &page->super);
    });
    if (failure.isEmpty())
        failure = rewrite(ctx, page, text, target, replacement, encoding);
    dropEncoding(ctx, *encoding);
    fz_drop_stext_page(ctx, text);
    pdf_drop_page(ctx, page);
    return failure;
}

} // namespace

EditOutcome replaceRun(Document &doc, int index, const TextRun &target, const QString &text)
{
    EditOutcome outcome;
    Encoding encoding;
    doc.modify(&outcome.error, [&](fz_context *ctx, pdf_document *pdf) {
        return edit(ctx, pdf, index, target, text, &encoding);
    });
    if (!outcome.error.isEmpty())
        return outcome;
    if (encoding.embedded)
        doc.markFontEmbedded();
    outcome.substitute = encoding.substitute;
    return outcome;
}
