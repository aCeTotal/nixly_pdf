#include "drawing.h"

namespace {

constexpr double kPageShare = 0.9;

struct Tracer
{
    fz_device super;
    PageText *found;
};

void tracePath(fz_context *ctx, fz_device *dev, const fz_path *path, int, fz_matrix ctm, fz_colorspace *,
               const float *, float alpha, fz_color_params)
{
    if (alpha <= 0)
        return;
    const fz_rect r = fz_bound_path(ctx, path, nullptr, ctm);
    reinterpret_cast<Tracer *>(dev)->found->backdrops.emplace_back(QPointF(r.x0, r.y0), QPointF(r.x1, r.y1));
}

void traceImage(fz_context *, fz_device *dev, fz_image *, fz_matrix ctm, float alpha, fz_color_params)
{
    if (alpha <= 0)
        return;
    const fz_rect r = fz_transform_rect(fz_unit_rect, ctm);
    const QRectF box(QPointF(r.x0, r.y0), QPointF(r.x1, r.y1));
    reinterpret_cast<Tracer *>(dev)->found->pictures.push_back({box, ctm});
}

} // namespace

void traceDrawing(fz_context *ctx, fz_page *page, PageText &found)
{
    auto *tracer = reinterpret_cast<Tracer *>(fz_new_device_of_size(ctx, sizeof(Tracer)));
    tracer->super.fill_path = tracePath;
    tracer->super.fill_image = traceImage;
    tracer->found = &found;
    fz_try(ctx)
    {
        fz_run_page(ctx, page, &tracer->super, fz_identity, nullptr);
        fz_close_device(ctx, &tracer->super);
    }
    fz_always(ctx)
        fz_drop_device(ctx, &tracer->super);
    fz_catch(ctx)
        fz_rethrow(ctx);
    const fz_rect bounds = fz_bound_page(ctx, page);
    const double width = (bounds.x1 - bounds.x0) * kPageShare;
    const double height = (bounds.y1 - bounds.y0) * kPageShare;
    std::erase_if(found.backdrops, [&](const QRectF &box) { return box.width() >= width && box.height() >= height; });
}
