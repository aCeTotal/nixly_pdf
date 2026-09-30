#include "markpaint.h"

#include "markshape.h"

#include <QPainter>
#include <QTextLayout>
#include <algorithm>

namespace {

constexpr double kTextInset = 2;
constexpr double kFirstBaseline = 1.0;
constexpr double kLineSpacing = 1.2;
const QString kCalloutFamily = QStringLiteral("Liberation Sans");

// Wrapped text on fixed baselines.
void paintText(QPainter &painter, const Mark &mark)
{
    QFont font(kCalloutFamily);
    font.setPixelSize(std::max(1, qRound(mark.size)));
    const double inset = mark.width / 2 + kTextInset;
    QString text = mark.text;
    QTextLayout layout(text.replace('\n', QChar::LineSeparator), font);
    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout.setTextOption(option);
    layout.beginLayout();
    for (double baseline = mark.size * kFirstBaseline;; baseline += mark.size * kLineSpacing) {
        QTextLine line = layout.createLine();
        if (!line.isValid())
            break;
        line.setLineWidth(std::max(0.0, mark.box.width() - 2 * inset));
        line.setPosition(QPointF(0, baseline - line.ascent()));
    }
    layout.endLayout();
    painter.setPen(mark.ink);
    painter.setClipRect(mark.box, Qt::IntersectClip);
    layout.draw(&painter, mark.box.topLeft() + QPointF(inset, 0));
}

} // namespace

void paintMark(QPainter &painter, const Mark &mark)
{
    if (mark.kind == MarkKind::Image) {
        painter.drawImage(mark.box, mark.image);
        return;
    }
    painter.save();
    const Qt::PenJoinStyle join = mark.kind == MarkKind::Cloud ? Qt::BevelJoin : Qt::MiterJoin;
    const QPen stroke(mark.ink, mark.width, Qt::SolidLine, Qt::FlatCap, join);
    painter.setPen(mark.width > 0 ? stroke : QPen(Qt::NoPen));
    painter.setBrush(mark.fill.isValid() ? QBrush(mark.fill) : QBrush());
    painter.drawPath(outlineOf(mark));
    painter.setBrush(mark.kind == MarkKind::Callout ? painter.brush() : QBrush(mark.ink));
    painter.drawPolygon(headOf(mark));
    if (mark.kind == MarkKind::Callout)
        paintText(painter, mark);
    painter.restore();
}
