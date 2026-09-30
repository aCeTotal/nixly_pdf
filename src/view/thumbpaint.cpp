#include "thumbstrip.h"

#include "app/theme.h"
#include "pdf/document.h"
#include "pdf/renderer.h"
#include "mark/markpaint.h"
#include "mark/markset.h"
#include "module/modulepaint.h"
#include "module/moduleset.h"

#include <QPainter>

namespace {

constexpr double kRadius = 4;
constexpr double kLiftScale = 1.06;
constexpr double kRestShadow = 40;
constexpr double kLiftShadow = 90;

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
    const int floating = drag && drag->lifted ? drag->from : landed;
    for (int i = slotAt(offset); i < int(tops.size()); ++i) {
        const double top = tops[size_t(i)] + shifts[size_t(i)].value() - offset;
        if (top > height())
            break;
        if (i != floating)
            paintThumb(painter, i, top, 0);
    }
    if (drag && drag->lifted)
        paintThumb(painter, drag->from, pointer - drag->grab, raise.value());
    else if (landed >= 0)
        paintThumb(painter, landed, tops[size_t(landed)] + shifts[size_t(landed)].value() - offset, raise.value());
}

void ThumbStrip::paintThumb(QPainter &painter, int index, double top, double raised)
{
    const QSizeF size = doc->slot(index).size;
    QRectF page((width() - kThumb) / 2, top, kThumb, kThumb * size.height() / size.width());
    const QPointF centre = page.center();
    page.setSize(page.size() * (1 + (kLiftScale - 1) * raised));
    page.moveCenter(centre);
    const bool active = index == current;
    paintShadow(painter, page, int(kRestShadow + (kLiftShadow - kRestShadow) * raised));
    painter.setBrush(Qt::white);
    painter.drawRoundedRect(page, kRadius, kRadius);
    const int id = doc->slot(index).id;
    if (const QImage *thumb = renderer->thumbnail(id))
        painter.drawImage(page, *thumb);
    painter.save();
    painter.translate(page.topLeft());
    painter.scale(page.width() / size.width(), page.width() / size.width());
    for (int module : layers.modules->onPage(id))
        paintModule(painter, *layers.modules->find(module), layers.modules->layout(module));
    for (int mark : layers.marks->onPage(id))
        paintMark(painter, *layers.marks->find(mark));
    painter.restore();
    if (active || raised > 0) {
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
