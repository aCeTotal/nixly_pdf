#include "signaturepreview.h"

#include "app/theme.h"
#include "signaturelayout.h"

#include <QPainter>
#include <algorithm>

namespace {

constexpr double kMaxScale = 1.6;
constexpr double kInset = 24;

} // namespace

SignaturePreview::SignaturePreview(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(460, 170);
}

void SignaturePreview::setSignature(const Signature &signature)
{
    shown = signature;
    update();
}

void SignaturePreview::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF card = QRectF(rect()).adjusted(1, 1, -1, -1);
    painter.setPen(QPen(theme::faded(theme::accent, 90), 1));
    painter.setBrush(Qt::white);
    painter.drawRoundedRect(card, 14, 14);
    painter.setPen(QPen(QColor(0, 0, 0, 40), 1, Qt::DashLine));
    const double line = card.bottom() - kInset;
    painter.drawLine(QPointF(card.left() + kInset * 2, line), QPointF(card.right() - kInset * 2, line));

    const SignatureLayout layout(shown);
    const QSizeF size = layout.size();
    if (size.isEmpty())
        return;
    const QRectF room = card.adjusted(kInset, kInset / 2, -kInset, -kInset / 2);
    const double scale = std::min({kMaxScale, room.width() / size.width(), room.height() / size.height()});
    painter.translate(room.center());
    painter.scale(scale, scale);
    layout.paint(painter);
}
