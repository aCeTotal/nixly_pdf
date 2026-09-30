#include "ocr.h"

#include "pdf/document.h"
#include "ocrlayout.h"

#include <algorithm>
#include <cmath>
#include <tesseract/capi.h>

namespace {

constexpr float kPoints = 72;
constexpr float kTargetWidth = 2550;
constexpr float kMinDpi = 70;
constexpr float kMaxDpi = 400;
constexpr float kMinConfidence = 55;
constexpr const char *kLanguages = "nor+eng";

struct Raster
{
    fz_pixmap *pix = nullptr;
    fz_rect bounds{};
    float dpi = kPoints;
};

Raster render(Document &doc, int index)
{
    fz_context *ctx = threadContext();
    std::lock_guard hold(doc.mutex());
    Raster raster;
    fz_page *page = nullptr;
    fz_var(page);
    fz_try(ctx)
    {
        page = fz_load_page(ctx, &doc.pdf()->super, index);
        raster.bounds = fz_bound_page(ctx, page);
        const float inches = (raster.bounds.x1 - raster.bounds.x0) / kPoints;
        raster.dpi = std::clamp(kTargetWidth / inches, kMinDpi, kMaxDpi);
        const float scale = raster.dpi / kPoints;
        raster.pix = fz_new_pixmap_from_page(ctx, page, fz_scale(scale, scale), fz_device_rgb(ctx), 0);
    }
    fz_always(ctx)
        fz_drop_page(ctx, page);
    fz_catch(ctx)
        fz_report_error(ctx);
    return raster;
}

OcrWord readWord(TessResultIterator *ri, TessPageIterator *pi, int line, int paragraph)
{
    char *utf8 = TessResultIteratorGetUTF8Text(ri, tesseract::RIL_WORD);
    const QString text = QString::fromUtf8(utf8).trimmed();
    TessDeleteText(utf8);
    int x1 = 0, y1 = 0, x2 = 0, y2 = 0, l = 0, t = 0, r = 0, b = 0;
    TessPageIteratorBaseline(pi, tesseract::RIL_WORD, &x1, &y1, &x2, &y2);
    TessPageIteratorBoundingBox(pi, tesseract::RIL_WORD, &l, &t, &r, &b);
    return {text, QRectF(QPointF(l, t), QPointF(r, b)), QPointF(l, y1),
            std::atan2(double(y2 - y1), double(x2 - x1)), line, paragraph};
}

std::vector<OcrWord> readWords(TessBaseAPI *api)
{
    std::vector<OcrWord> words;
    TessResultIterator *ri = TessBaseAPIGetIterator(api);
    if (!ri)
        return words;
    TessPageIterator *pi = TessResultIteratorGetPageIterator(ri);
    int line = -1;
    int paragraph = -1;
    do {
        paragraph += TessPageIteratorIsAtBeginningOf(pi, tesseract::RIL_PARA) ? 1 : 0;
        line += TessPageIteratorIsAtBeginningOf(pi, tesseract::RIL_TEXTLINE) ? 1 : 0;
        OcrWord word = readWord(ri, pi, line, paragraph);
        const bool sure = TessResultIteratorConfidence(ri, tesseract::RIL_WORD) >= kMinConfidence;
        if (sure && !word.text.isEmpty())
            words.push_back(std::move(word));
    } while (TessPageIteratorNext(pi, tesseract::RIL_WORD));
    TessResultIteratorDelete(ri);
    return words;
}

} // namespace

OcrResult recognize(Document &doc, int index)
{
    OcrResult result;
    fz_context *ctx = threadContext();
    const Raster raster = render(doc, index);
    if (!raster.pix) {
        result.error = QStringLiteral("Could not render the page");
        return result;
    }
    const OcrImage image{fz_pixmap_samples(ctx, raster.pix),
                         fz_pixmap_width(ctx, raster.pix),
                         fz_pixmap_height(ctx, raster.pix),
                         int(fz_pixmap_stride(ctx, raster.pix)),
                         QPointF(raster.bounds.x0, raster.bounds.y0),
                         kPoints / raster.dpi};
    TessBaseAPI *api = TessBaseAPICreate();
    if (TessBaseAPIInit3(api, nullptr, kLanguages) == 0) {
        TessBaseAPISetImage(api, image.samples, image.width, image.height, 3, image.stride);
        TessBaseAPISetSourceResolution(api, int(raster.dpi));
        if (TessBaseAPIRecognize(api, nullptr) == 0)
            result.blocks = layoutBlocks(readWords(api), image);
    } else {
        result.error = QStringLiteral("Tesseract language data is missing");
    }
    TessBaseAPIEnd(api);
    TessBaseAPIDelete(api);
    fz_drop_pixmap(ctx, raster.pix);
    return result;
}
