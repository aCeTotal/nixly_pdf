#include "annotate.h"

#include "markshape.h"
#include "pdf/document.h"

#include <QBuffer>

namespace {

constexpr const char *kCalloutFont = "Helv";
constexpr const char *kImageStamp = "Image";
constexpr int kRgb = 3;
constexpr int kJpegQuality = 90;

struct Rgb
{
    float v[kRgb];
};

Rgb rgbOf(const QColor &color)
{
    return {{float(color.redF()), float(color.greenF()), float(color.blueF())}};
}

fz_rect rectOf(const QRectF &box, double grow)
{
    return {float(box.left() - grow), float(box.top() - grow), float(box.right() + grow), float(box.bottom() + grow)};
}

fz_point pointOf(QPointF p)
{
    return fz_make_point(float(p.x()), float(p.y()));
}

enum pdf_annot_type typeOf(MarkKind kind)
{
    switch (kind) {
    case MarkKind::Ellipse:
        return PDF_ANNOT_CIRCLE;
    case MarkKind::Arrow:
        return PDF_ANNOT_LINE;
    case MarkKind::Callout:
        return PDF_ANNOT_FREE_TEXT;
    case MarkKind::Image:
        return PDF_ANNOT_STAMP;
    case MarkKind::Rectangle:
    case MarkKind::Cloud:
        return PDF_ANNOT_SQUARE;
    }
    return PDF_ANNOT_SQUARE;
}

void describeShape(fz_context *ctx, pdf_annot *annot, const Mark &mark)
{
    const Rgb ink = rgbOf(mark.ink);
    pdf_set_annot_rect(ctx, annot, rectOf(mark.box, mark.width / 2));
    pdf_set_annot_color(ctx, annot, kRgb, ink.v);
    pdf_set_annot_border_width(ctx, annot, float(mark.width));
    if (mark.fill.isValid()) {
        const Rgb fill = rgbOf(mark.fill);
        pdf_set_annot_interior_color(ctx, annot, kRgb, fill.v);
    }
    if (mark.kind != MarkKind::Cloud)
        return;
    pdf_set_annot_border_effect(ctx, annot, PDF_BORDER_EFFECT_CLOUDY);
    pdf_set_annot_border_effect_intensity(ctx, annot, kCloudIntensity);
}

void describeArrow(fz_context *ctx, pdf_annot *annot, const Mark &mark)
{
    const Rgb ink = rgbOf(mark.ink);
    pdf_set_annot_line(ctx, annot, pointOf(mark.tail), pointOf(mark.tip));
    pdf_set_annot_line_ending_styles(ctx, annot, PDF_ANNOT_LE_NONE, PDF_ANNOT_LE_CLOSED_ARROW);
    pdf_set_annot_intent(ctx, annot, PDF_ANNOT_IT_LINE_ARROW);
    pdf_set_annot_color(ctx, annot, kRgb, ink.v);
    pdf_set_annot_interior_color(ctx, annot, kRgb, ink.v);
    pdf_set_annot_border_width(ctx, annot, float(mark.width));
}

void describeCallout(fz_context *ctx, pdf_annot *annot, const Mark &mark)
{
    const Rgb ink = rgbOf(mark.ink);
    pdf_set_annot_rect(ctx, annot, rectOf(mark.box, 0));
    pdf_set_annot_contents(ctx, annot, mark.text.toUtf8().constData());
    pdf_set_annot_default_appearance(ctx, annot, kCalloutFont, float(mark.size), kRgb, ink.v);
    pdf_set_annot_border_width(ctx, annot, float(mark.width));
    if (mark.fill.isValid()) {
        const Rgb fill = rgbOf(mark.fill);
        pdf_set_annot_color(ctx, annot, kRgb, fill.v);
    }
    if (mark.box.contains(mark.tip))
        return;
    fz_point leader[3] = {pointOf(mark.tip), pointOf(attachOf(mark)), {}};
    pdf_set_annot_intent(ctx, annot, PDF_ANNOT_IT_FREETEXT_CALLOUT);
    pdf_set_annot_callout_line(ctx, annot, leader, 2);
    pdf_set_annot_callout_style(ctx, annot, PDF_ANNOT_LE_CLOSED_ARROW);
}

// Original bytes, else a fresh encoding.
QByteArray encodingOf(const Mark &mark)
{
    if (!mark.encoded.isEmpty())
        return mark.encoded;
    QByteArray bytes;
    QBuffer sink(&bytes);
    sink.open(QIODevice::WriteOnly);
    mark.image.save(&sink, mark.image.hasAlphaChannel() ? "PNG" : "JPG", kJpegQuality);
    return bytes;
}

void describeImage(fz_context *ctx, pdf_annot *annot, const Mark &mark)
{
    const QByteArray file = encodingOf(mark);
    const auto *bytes = reinterpret_cast<const unsigned char *>(file.constData());
    fz_buffer *data = fz_new_buffer_from_copied_data(ctx, bytes, size_t(file.size()));
    fz_image *image = nullptr;
    fz_var(image);
    fz_try(ctx)
    {
        image = fz_new_image_from_buffer(ctx, data);
        pdf_set_annot_rect(ctx, annot, rectOf(mark.box, 0));
        pdf_set_annot_icon_name(ctx, annot, kImageStamp);
        pdf_set_annot_stamp_image(ctx, annot, image);
    }
    fz_always(ctx)
    {
        fz_drop_image(ctx, image);
        fz_drop_buffer(ctx, data);
    }
    fz_catch(ctx)
        fz_rethrow(ctx);
}

void describe(fz_context *ctx, pdf_annot *annot, const Mark &mark)
{
    switch (mark.kind) {
    case MarkKind::Arrow:
        describeArrow(ctx, annot, mark);
        return;
    case MarkKind::Callout:
        describeCallout(ctx, annot, mark);
        return;
    case MarkKind::Image:
        describeImage(ctx, annot, mark);
        return;
    case MarkKind::Rectangle:
    case MarkKind::Ellipse:
    case MarkKind::Cloud:
        describeShape(ctx, annot, mark);
        return;
    }
}

void writeMark(fz_context *ctx, pdf_page *page, const Mark &mark)
{
    pdf_annot *annot = pdf_create_annot(ctx, page, typeOf(mark.kind));
    fz_try(ctx)
        describe(ctx, annot, mark);
    fz_always(ctx)
        pdf_drop_annot(ctx, annot);
    fz_catch(ctx)
        fz_rethrow(ctx);
}

void writeMarks(fz_context *ctx, pdf_page *page, const std::vector<const Mark *> &marks)
{
    for (const Mark *mark : marks)
        writeMark(ctx, page, *mark);
    pdf_update_page(ctx, page);
}

// Writes marks, drops the page.
void writePage(fz_context *ctx, pdf_page *page, const std::vector<const Mark *> &marks)
{
    fz_try(ctx)
        writeMarks(ctx, page, marks);
    fz_always(ctx)
        pdf_drop_page(ctx, page);
    fz_catch(ctx)
        fz_rethrow(ctx);
}

} // namespace

QString annotate(fz_context *ctx, pdf_document *copy, const Annotating &source)
{
    std::vector<std::vector<const Mark *>> pages(size_t(source.doc.count()));
    for (size_t index = 0; index < pages.size(); ++index) {
        for (int id : source.marks.onPage(source.doc.slot(int(index)).id))
            pages[index].push_back(source.marks.find(id));
    }
    return attempt(ctx, [&] {
        for (size_t index = 0; index < pages.size(); ++index) {
            if (!pages[index].empty())
                writePage(ctx, pdf_load_page(ctx, copy, int(index)), pages[index]);
        }
    });
}
