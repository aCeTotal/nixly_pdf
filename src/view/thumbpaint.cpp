#include "thumbstrip.h"

#include "app/theme.h"
#include "pdf/document.h"
#include "pdf/renderer.h"

#include <QPainter>

namespace {

constexpr double kRadius = 4;
constexpr double kLiftScale = 1.06;

void paintShadow(QPainter &painter, const QRectF &rect, int strength)
{
    painter.setPen(Qt::NoPen);
    for (int i = 3; i > 0; --i) {
        painter.setBrush(QColor(0, 0, 0, strength / i));
        painter.drawRoundedRect(rect.adjusted(-i, -i + 2, i, i + 2), kRadius + i, kRadius + i);
    }
}

} // namespace

void ThumbStrip::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), theme::surface);
    painter.setPen(theme::faded(theme::text, 18));
    painter.drawLine(QPointF(width() - 0.5, 0), QPointF(width() - 0.5, height()));
    if (!doc || tops.empty())
        return;
    renderer->setThumbnailWidth(int(kThumb * devicePixelRatioF()));
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    const double offset = scroll.value();
    for (int i = slotAt(offset); i < int(tops.size()); ++i) {
        const double top = tops[size_t(i)] + shifts[size_t(i)].value() - offset;
        if (top > height())
            break;
        if (!(drag && drag->lifted && drag->from == i))
            paintThumb(painter, i, top, false);
    }
    if (drag && drag->lifted)
        paintThumb(painter, drag->from, pointer - drag->grab, true);
}

void ThumbStrip::paintThumb(QPainter &painter, int index, double top, bool lifted)
{
    const QSizeF size = doc->slot(index).size;
    QRectF page((width() - kThumb) / 2, top, kThumb, kThumb * size.height() / size.width());
    if (lifted) {
        const QPointF centre = page.center();
        page.setSize(page.size() * kLiftScale);
        page.moveCenter(centre);
    }
    const bool active = index == current;
    paintShadow(painter, page, lifted ? 90 : 40);
    painter.setBrush(Qt::white);
    painter.drawRoundedRect(page, kRadius, kRadius);
    if (const QImage *thumb = renderer->thumbnail(doc->slot(index).id))
        painter.drawImage(page, *thumb);
    if (active || lifted) {
        painter.setPen(QPen(theme::accent, 2.5));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(page.adjusted(-4, -4, 4, 4), kRadius + 3, kRadius + 3);
    }
    QFont font = painter.font();
    font.setPixelSize(12);
    font.setWeight(active ? QFont::DemiBold : QFont::Normal);
    painter.setFont(font);
    painter.setPen(active ? theme::accent : theme::muted);
    painter.drawText(QRectF(0, page.bottom() + 6, width(), 16), Qt::AlignHCenter | Qt::AlignTop,
                     QString::number(index + 1));
}
