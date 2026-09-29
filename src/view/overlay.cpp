#include "overlay.h"

#include <QFontMetricsF>
#include "app/theme.h"

#include <QPainter>
#include <algorithm>

namespace {

constexpr double kHoldSeconds = 1.1;
constexpr double kMinThumb = 36;
constexpr double kThumbWidth = 6;
constexpr double kBadgeHeight = 30;

} // namespace

void ScrollOverlay::wake()
{
    hold = kHoldSeconds;
    opacity.setTarget(1);
}

bool ScrollOverlay::advance(double seconds)
{
    hold = std::max(0.0, hold - seconds);
    if (hold == 0)
        opacity.setTarget(0);
    opacity.advance(seconds);
    return hold > 0 || opacity.moving();
}

QRectF ScrollOverlay::thumb(const ScrollState &state) const
{
    const QRectF &view = state.viewport;
    if (state.extent <= view.height())
        return {};
    const double track = view.height() - 8;
    const double length = std::max(kMinThumb, track * view.height() / state.extent);
    const double travel = state.offset / (state.extent - view.height());
    return QRectF(view.right() - kThumbWidth - 4, view.top() + 4 + travel * (track - length), kThumbWidth, length);
}

void ScrollOverlay::paint(QPainter &painter, const ScrollState &state, const QString &badge) const
{
    const double alpha = opacity.value();
    if (alpha <= 0)
        return;
    painter.save();
    painter.setOpacity(alpha);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(theme::faded(theme::text, 150));
    const QRectF bar = thumb(state);
    painter.drawRoundedRect(bar, kThumbWidth / 2, kThumbWidth / 2);

    QFont font = painter.font();
    font.setPixelSize(13);
    font.setWeight(QFont::DemiBold);
    painter.setFont(font);
    const double width = QFontMetricsF(font).horizontalAdvance(badge) + 28;
    const QRectF pill(state.viewport.center().x() - width / 2, state.viewport.bottom() - kBadgeHeight - 22, width,
                      kBadgeHeight);
    painter.setBrush(theme::faded(theme::surface, 220));
    painter.setPen(theme::faded(theme::accent, 110));
    painter.drawRoundedRect(pill, kBadgeHeight / 2, kBadgeHeight / 2);
    painter.setPen(theme::text);
    painter.drawText(pill, Qt::AlignCenter, badge);
    painter.restore();
}
