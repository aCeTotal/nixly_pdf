#include "pagepainter.h"

#include "pdf/renderer.h"

#include <QPainter>
#include <cmath>
#include <vector>

namespace {

constexpr int kTile = Renderer::kTileSize;
constexpr int kShadowLayers = 4;
constexpr QColor kPaper(255, 255, 255);

double snap(double v, double dpr)
{
    return std::round(v * dpr) / dpr;
}

// Visits visible tiles at scale.
template <typename Visit>
void forEachTile(const PageFrame &frame, int key, Visit visit)
{
    const double renderScale = Renderer::keyScale(key);
    const bool exact = key == Renderer::scaleKey(frame.zoom * frame.dpr);
    const double ratio = exact ? 1.0 / frame.dpr : frame.zoom / renderScale;
    const QSizeF points = frame.rect.size() / frame.zoom;
    const int fullWidth = int(std::ceil(points.width() * renderScale));
    const int fullHeight = int(std::ceil(points.height() * renderScale));
    const QRectF area = frame.clip.intersected(frame.rect).translated(-frame.rect.topLeft());
    if (area.isEmpty() || fullWidth <= 0 || fullHeight <= 0)
        return;
    const double span = ratio * kTile;
    const int c0 = int(area.left() / span), c1 = std::min((fullWidth - 1) / kTile, int(area.right() / span));
    const int r0 = int(area.top() / span), r1 = std::min((fullHeight - 1) / kTile, int(area.bottom() / span));
    const double left = snap(frame.rect.left(), frame.dpr);
    const double top = snap(frame.rect.top(), frame.dpr);
    for (int r = r0; r <= r1; ++r) {
        for (int c = c0; c <= c1; ++c) {
            const double x0 = snap(left + c * span, frame.dpr);
            const double y0 = snap(top + r * span, frame.dpr);
            const double x1 = snap(left + std::min(fullWidth, (c + 1) * kTile) * ratio, frame.dpr);
            const double y1 = snap(top + std::min(fullHeight, (r + 1) * kTile) * ratio, frame.dpr);
            visit(TileKey{frame.id, key, c, r}, QRectF(x0, y0, x1 - x0, y1 - y0));
        }
    }
}

void paintShadow(QPainter &painter, const QRectF &rect)
{
    painter.setPen(Qt::NoPen);
    for (int i = kShadowLayers; i > 0; --i) {
        painter.setBrush(QColor(0, 0, 0, 22 - i * 4));
        painter.drawRoundedRect(rect.adjusted(-i * 2, -i * 2 + 3, i * 2, i * 2 + 3), i * 2 + 2, i * 2 + 2);
    }
}

void paintPreview(QPainter &painter, Renderer &renderer, const PageFrame &frame)
{
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    if (const QImage *thumb = renderer.thumbnail(frame.id))
        painter.drawImage(frame.rect, *thumb);
    if (frame.fallback <= 0 || frame.fallback == frame.scale)
        return;
    forEachTile(frame, frame.fallback, [&](const TileKey &key, const QRectF &target) {
        if (const QImage *image = renderer.cachedTile(key))
            painter.drawImage(target, *image);
    });
}

} // namespace

void paintPage(QPainter &painter, Renderer &renderer, const PageFrame &frame)
{
    struct Placed
    {
        const QImage *image;
        QRectF target;
    };
    std::vector<Placed> sharp;
    bool complete = true;
    forEachTile(frame, frame.scale, [&](const TileKey &key, const QRectF &target) {
        const QImage *image = renderer.tile(key);
        complete = complete && image;
        if (image)
            sharp.push_back({image, target});
    });

    paintShadow(painter, frame.rect);
    painter.fillRect(frame.rect, kPaper);
    if (!complete)
        paintPreview(painter, renderer, frame);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, frame.scale != Renderer::scaleKey(frame.zoom * frame.dpr));
    for (const Placed &tile : sharp)
        painter.drawImage(tile.target, *tile.image);
}

void prefetchPage(Renderer &renderer, const PageFrame &frame)
{
    forEachTile(frame, frame.scale, [&](const TileKey &key, const QRectF &) { renderer.tile(key); });
}
