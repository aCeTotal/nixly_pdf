#include "pickmarks.h"

#include "pdf/document.h"

#include <algorithm>
#include <optional>

namespace {

constexpr int kCorners = 4;

enum Channels { kGray = 1, kRgb = 3, kCmyk = 4 };

QColor colourOf(int n, const float c[4])
{
    switch (n) {
    case kGray:
        return QColor::fromRgbF(c[0], c[0], c[0]);
    case kRgb:
        return QColor::fromRgbF(c[0], c[1], c[2]);
    case kCmyk:
        return QColor::fromCmykF(c[0], c[1], c[2], c[3]);
    default:
        return {};
    }
}

// Rect inside half the border.
QRectF innerOf(fz_context *ctx, pdf_annot *annot, double width)
{
    const fz_rect r = pdf_annot_rect(ctx, annot);
    const double half = width / 2;
    return QRectF(QPointF(r.x0 + half, r.y0 + half), QPointF(r.x1 - half, r.y1 - half)).normalized();
}

Mark styled(fz_context *ctx, pdf_annot *annot, MarkKind kind)
{
    Mark mark;
    mark.kind = kind;
    mark.width = pdf_annot_border_width(ctx, annot);
    int n = 0;
    float c[4] = {};
    pdf_annot_color(ctx, annot, &n, c);
    mark.ink = colourOf(n, c);
    if (!pdf_annot_has_interior_color(ctx, annot))
        return mark;
    pdf_annot_interior_color(ctx, annot, &n, c);
    mark.fill = colourOf(n, c);
    return mark;
}

bool cloudy(fz_context *ctx, pdf_annot *annot)
{
    return pdf_annot_has_border_effect(ctx, annot) &&
           pdf_annot_border_effect(ctx, annot) == PDF_BORDER_EFFECT_CLOUDY;
}

Mark boxed(fz_context *ctx, pdf_annot *annot, MarkKind kind)
{
    Mark mark = styled(ctx, annot, kind);
    mark.box = innerOf(ctx, annot, mark.width);
    return mark;
}

// Cloud drawn as upright rectangle.
std::optional<Mark> cloudOf(fz_context *ctx, pdf_annot *annot)
{
    if (!cloudy(ctx, annot) || pdf_annot_vertex_count(ctx, annot) != kCorners)
        return std::nullopt;
    QRectF box;
    for (int i = 0; i < kCorners; ++i) {
        const fz_point p = pdf_annot_vertex(ctx, annot, i);
        box = i ? box.united(QRectF(p.x, p.y, 0, 0)) : QRectF(p.x, p.y, 0, 0);
    }
    for (int i = 0; i < kCorners; ++i) {
        const fz_point p = pdf_annot_vertex(ctx, annot, i);
        const bool corner = (p.x == box.left() || p.x == box.right()) && (p.y == box.top() || p.y == box.bottom());
        if (!corner)
            return std::nullopt;
    }
    Mark mark = styled(ctx, annot, MarkKind::Cloud);
    mark.box = box;
    return mark;
}

bool pointed(pdf_line_ending end)
{
    return end == PDF_ANNOT_LE_CLOSED_ARROW || end == PDF_ANNOT_LE_OPEN_ARROW;
}

std::optional<Mark> arrowOf(fz_context *ctx, pdf_annot *annot)
{
    pdf_line_ending start = PDF_ANNOT_LE_NONE;
    pdf_line_ending end = PDF_ANNOT_LE_NONE;
    pdf_annot_line_ending_styles(ctx, annot, &start, &end);
    if (!pointed(start) && !pointed(end))
        return std::nullopt;
    fz_point a;
    fz_point b;
    pdf_annot_line(ctx, annot, &a, &b);
    Mark mark = styled(ctx, annot, MarkKind::Arrow);
    mark.fill = QColor();
    mark.tail = pointed(end) ? QPointF(a.x, a.y) : QPointF(b.x, b.y);
    mark.tip = pointed(end) ? QPointF(b.x, b.y) : QPointF(a.x, a.y);
    return mark;
}

Mark calloutOf(fz_context *ctx, pdf_annot *annot)
{
    Mark mark;
    mark.kind = MarkKind::Callout;
    mark.width = pdf_annot_border_width(ctx, annot);
    const char *font = nullptr;
    float size = 0;
    int n = 0;
    float c[4] = {};
    pdf_annot_default_appearance(ctx, annot, &font, &size, &n, c);
    mark.ink = colourOf(n, c);
    mark.size = size > 0 ? size : mark.size;
    pdf_annot_color(ctx, annot, &n, c);
    mark.fill = colourOf(n, c);
    mark.text = QString::fromUtf8(pdf_annot_contents(ctx, annot));
    mark.box = innerOf(ctx, annot, mark.width);
    mark.tip = mark.box.center();
    if (!pdf_annot_has_callout(ctx, annot))
        return mark;
    fz_point leader[3];
    int count = 0;
    pdf_annot_callout_line(ctx, annot, leader, &count);
    mark.tip = count ? QPointF(leader[0].x, leader[0].y) : mark.tip;
    return mark;
}

// RGB pixels, alpha kept.
fz_pixmap *rgbOf(fz_context *ctx, fz_image *image)
{
    fz_pixmap *source = fz_get_pixmap_from_image(ctx, image, nullptr, nullptr, nullptr, nullptr);
    fz_pixmap *rgb = nullptr;
    fz_var(rgb);
    fz_try(ctx)
        rgb = fz_convert_pixmap(ctx, source, fz_device_rgb(ctx), nullptr, nullptr, fz_default_color_params, 1);
    fz_always(ctx)
        fz_drop_pixmap(ctx, source);
    fz_catch(ctx)
        fz_rethrow(ctx);
    return rgb;
}

QImage pictureOf(fz_context *ctx, fz_pixmap *pix)
{
    const QImage::Format format =
        fz_pixmap_alpha(ctx, pix) ? QImage::Format_RGBA8888_Premultiplied : QImage::Format_RGB888;
    const QImage view(fz_pixmap_samples(ctx, pix), fz_pixmap_width(ctx, pix), fz_pixmap_height(ctx, pix),
                      int(fz_pixmap_stride(ctx, pix)), format);
    return view.copy();
}

// Picture held by image stamp.
std::optional<Mark> imageOf(fz_context *ctx, pdf_annot *annot)
{
    pdf_obj *picture = pdf_annot_stamp_image_obj(ctx, annot);
    if (!picture)
        return std::nullopt;
    fz_image *image = pdf_load_image(ctx, pdf_annot_page(ctx, annot)->doc, picture);
    fz_pixmap *pix = nullptr;
    fz_var(pix);
    fz_try(ctx)
        pix = rgbOf(ctx, image);
    fz_always(ctx)
        fz_drop_image(ctx, image);
    fz_catch(ctx)
        fz_rethrow(ctx);
    Mark mark;
    mark.kind = MarkKind::Image;
    mark.box = innerOf(ctx, annot, 0);
    mark.image = pictureOf(ctx, pix);
    fz_drop_pixmap(ctx, pix);
    return mark;
}

std::optional<Mark> markOf(fz_context *ctx, pdf_annot *annot)
{
    switch (pdf_annot_type(ctx, annot)) {
    case PDF_ANNOT_SQUARE:
        return boxed(ctx, annot, cloudy(ctx, annot) ? MarkKind::Cloud : MarkKind::Rectangle);
    case PDF_ANNOT_CIRCLE:
        return boxed(ctx, annot, MarkKind::Ellipse);
    case PDF_ANNOT_POLYGON:
        return cloudOf(ctx, annot);
    case PDF_ANNOT_LINE:
        return arrowOf(ctx, annot);
    case PDF_ANNOT_FREE_TEXT:
        return calloutOf(ctx, annot);
    case PDF_ANNOT_STAMP:
        return imageOf(ctx, annot);
    default:
        return std::nullopt;
    }
}

// Lifts marks off one page.
void pickInto(fz_context *ctx, pdf_page *page, std::vector<Mark> &marks)
{
    for (pdf_annot *annot = pdf_first_annot(ctx, page); annot;) {
        pdf_annot *next = pdf_next_annot(ctx, annot);
        std::optional<Mark> mark = markOf(ctx, annot);
        if (mark) {
            marks.push_back(std::move(*mark));
            pdf_delete_annot(ctx, page, annot);
        }
        annot = next;
    }
}

// Lifts marks, drops the page.
void pickPage(fz_context *ctx, pdf_page *page, std::vector<Mark> &marks)
{
    fz_try(ctx)
        pickInto(ctx, page, marks);
    fz_always(ctx)
        pdf_drop_page(ctx, page);
    fz_catch(ctx)
        fz_rethrow(ctx);
}

} // namespace

std::vector<Mark> pickMarks(Document &doc)
{
    std::vector<Mark> marks;
    QString error;
    doc.modify(&error, [&](fz_context *ctx, pdf_document *pdf) {
        return attempt(ctx, [&] {
            for (int index = 0; index < doc.count(); ++index) {
                const auto fresh = qsizetype(marks.size());
                pickPage(ctx, pdf_load_page(ctx, pdf, index), marks);
                const int id = doc.slot(index).id;
                std::for_each(marks.begin() + fresh, marks.end(), [id](Mark &mark) { mark.page = id; });
            }
        });
    });
    return marks;
}
