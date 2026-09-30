#include "redraw.h"

#include "pdf/context.h"

#include <QFile>
#include <algorithm>

namespace {

constexpr float kA4Width = 595;
constexpr float kA4Height = 842;
constexpr float kEm = 11;
constexpr const char *kWriterOptions = "compress,compress-images,compress-fonts";

// Draws a page, drops it.
void copyPage(fz_context *ctx, fz_page *page, fz_document_writer *writer)
{
    fz_try(ctx)
    {
        const fz_rect bounds = fz_bound_page(ctx, page);
        const float scale = std::min(1.0f, kA4Height / std::max(bounds.x1 - bounds.x0, bounds.y1 - bounds.y0));
        const fz_matrix fit = fz_scale(scale, scale);
        fz_device *dev = fz_begin_page(ctx, writer, fz_transform_rect(bounds, fit));
        fz_run_page(ctx, page, dev, fit, nullptr);
        fz_end_page(ctx, writer);
    }
    fz_always(ctx)
        fz_drop_page(ctx, page);
    fz_catch(ctx)
        fz_rethrow(ctx);
}

} // namespace

QString redrawAsPdf(const QString &source, const QString &target)
{
    fz_context *ctx = threadContext();
    const QByteArray from = QFile::encodeName(source);
    const QByteArray to = QFile::encodeName(target);
    fz_document *doc = nullptr;
    fz_document_writer *writer = nullptr;
    const QString failure = attempt(ctx, [&] {
        doc = fz_open_document(ctx, from.constData());
        if (fz_is_document_reflowable(ctx, doc))
            fz_layout_document(ctx, doc, kA4Width, kA4Height, kEm);
        writer = fz_new_pdf_writer(ctx, to.constData(), kWriterOptions);
        for (int i = 0; i < fz_count_pages(ctx, doc); ++i)
            copyPage(ctx, fz_load_page(ctx, doc, i), writer);
        fz_close_document_writer(ctx, writer);
    });
    fz_drop_document_writer(ctx, writer);
    fz_drop_document(ctx, doc);
    return failure;
}
