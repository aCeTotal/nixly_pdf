#include "picture.h"

#include "content.h"
#include "document.h"

namespace {

// PDF image space is flipped.
constexpr fz_matrix kImageFlip{1, 0, 0, -1, 0, 1};

// Finds image at a placement.
struct Catcher
{
    fz_device super;
    fz_matrix placement;
    fz_image *image;
};

struct Taking
{
    int index;
    const Picture &picture;
    Taken &taken;
};

void catchImage(fz_context *ctx, fz_device *dev, fz_image *image, fz_matrix ctm, float, fz_color_params)
{
    auto *catcher = reinterpret_cast<Catcher *>(dev);
    if (!catcher->image && samePlacement(ctm, catcher->placement))
        catcher->image = fz_keep_image(ctx, image);
}

fz_image *findImage(fz_context *ctx, fz_page *page, fz_matrix placement)
{
    auto *catcher = reinterpret_cast<Catcher *>(fz_new_device_of_size(ctx, sizeof(Catcher)));
    catcher->super.fill_image = catchImage;
    catcher->placement = placement;
    fz_image *found = nullptr;
    fz_var(found);
    fz_try(ctx)
    {
        fz_run_page(ctx, page, &catcher->super, fz_identity, nullptr);
        fz_close_device(ctx, &catcher->super);
    }
    fz_always(ctx)
    {
        found = catcher->image;
        fz_drop_device(ctx, &catcher->super);
    }
    fz_catch(ctx)
    {
        fz_drop_image(ctx, found);
        fz_rethrow(ctx);
    }
    return found;
}

// RGB decode, alpha kept.
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

QString takeFrom(fz_context *ctx, pdf_document *pdf, const Taking &job)
{
    pdf_page *page = nullptr;
    fz_image *image = nullptr;
    fz_matrix ctm = fz_identity;
    QString failure = attempt(ctx, [&] {
        page = pdf_load_page(ctx, pdf, job.index);
        pdf_page_transform(ctx, page, nullptr, &ctm);
        image = findImage(ctx, &page->super, job.picture.placement);
    });
    if (failure.isEmpty() && !image)
        failure = QStringLiteral("The picture moved; try again");
    if (failure.isEmpty())
        job.taken.image = decodeImage(ctx, image);
    if (failure.isEmpty() && job.taken.image.isNull())
        failure = QStringLiteral("The picture could not be decoded");
    if (failure.isEmpty()) {
        job.taken.encoded = jpegOf(ctx, image);
        const fz_matrix user = fz_concat(fz_concat(kImageFlip, job.picture.placement), fz_invert_matrix(ctm));
        failure = rewritePage(ctx, page, {{}, {}, {}, {user}});
    }
    fz_drop_image(ctx, image);
    pdf_drop_page(ctx, page);
    return failure;
}

} // namespace

QImage decodeImage(fz_context *ctx, fz_image *image)
{
    fz_pixmap *pix = nullptr;
    if (!attempt(ctx, [&] { pix = rgbOf(ctx, image); }).isEmpty())
        return {};
    const QImage::Format format =
        fz_pixmap_alpha(ctx, pix) ? QImage::Format_RGBA8888_Premultiplied : QImage::Format_RGB888;
    const QImage view(fz_pixmap_samples(ctx, pix), fz_pixmap_width(ctx, pix), fz_pixmap_height(ctx, pix),
                      int(fz_pixmap_stride(ctx, pix)), format);
    QImage copy = view.copy();
    fz_drop_pixmap(ctx, pix);
    return copy;
}

QByteArray jpegOf(fz_context *ctx, fz_image *image)
{
    fz_compressed_buffer *compressed = fz_compressed_image_buffer(ctx, image);
    if (image->mask || !compressed || compressed->params.type != FZ_IMAGE_JPEG)
        return {};
    unsigned char *data = nullptr;
    const size_t size = fz_buffer_storage(ctx, compressed->buffer, &data);
    return QByteArray(reinterpret_cast<const char *>(data), qsizetype(size));
}

Taken takePicture(Document &doc, int index, const Picture &picture)
{
    Taken taken;
    doc.modify(&taken.error,
               [&](fz_context *ctx, pdf_document *pdf) { return takeFrom(ctx, pdf, {index, picture, taken}); });
    return taken;
}
