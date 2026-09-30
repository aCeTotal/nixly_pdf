#include "markshape.h"

#include <QLineF>
#include <QPainterPathStroker>
#include <algorithm>
#include <array>
#include <cmath>

namespace {

constexpr double kScallopRadius = 3.75 * kCloudIntensity;
constexpr double kHeadRatio = 8.8;
constexpr double kHeadSpread = 0.51;
constexpr double kShaftInset = 0.7;
constexpr double kHalfTurn = 180;

// Box edges a handle moves.
struct Sides
{
    bool left;
    bool top;
    bool right;
    bool bottom;
};

constexpr std::array<Sides, 8> kSides{{
    {true, true, false, false},
    {false, true, false, false},
    {false, true, true, false},
    {false, false, true, false},
    {false, false, true, true},
    {false, false, false, true},
    {true, false, false, true},
    {true, false, false, false},
}};

double headLength(double width)
{
    return width * kHeadRatio;
}

// Arcs bulging out per edge.
QPainterPath cloudAround(const QRectF &box, double radius)
{
    const std::array<QPointF, 5> corners{box.topLeft(), box.topRight(), box.bottomRight(), box.bottomLeft(),
                                         box.topLeft()};
    constexpr std::array<double, 4> starts{180, 90, 0, 270};
    QPainterPath path(corners[0]);
    for (size_t edge = 0; edge < starts.size(); ++edge) {
        const QPointF span = corners[edge + 1] - corners[edge];
        const int count = std::max(1, int(std::lround(std::hypot(span.x(), span.y()) / (2 * radius))));
        const QPointF step = span / count;
        const double half = std::hypot(step.x(), step.y()) / 2;
        for (int i = 0; i < count; ++i) {
            const QPointF mid = corners[edge] + step * (i + 0.5);
            path.arcTo(QRectF(mid - QPointF(half, half), QSizeF(2 * half, 2 * half)), starts[edge], -kHalfTurn);
        }
    }
    path.closeSubpath();
    return path;
}

// Shaft stops inside the head.
QPointF shaftEnd(QPointF tail, QPointF tip, double width)
{
    QLineF shaft(tail, tip);
    const double inset = headLength(width) * kShaftInset;
    if (shaft.length() <= inset)
        return tail;
    shaft.setLength(shaft.length() - inset);
    return shaft.p2();
}

QPolygonF arrowHead(QPointF tail, QPointF tip, double width)
{
    const double distance = QLineF(tail, tip).length();
    if (distance <= 0)
        return {};
    const QPointF back = (tail - tip) / distance * headLength(width);
    const QPointF side(-back.y() * kHeadSpread, back.x() * kHeadSpread);
    return QPolygonF({tip, tip + back + side, tip + back - side});
}

} // namespace

QPainterPath outlineOf(const Mark &mark)
{
    QPainterPath path;
    switch (mark.kind) {
    case MarkKind::Ellipse:
        path.addEllipse(mark.box);
        return path;
    case MarkKind::Cloud:
        return cloudAround(mark.box, kScallopRadius);
    case MarkKind::Arrow:
        path.moveTo(mark.tail);
        path.lineTo(shaftEnd(mark.tail, mark.tip, mark.width));
        return path;
    case MarkKind::Callout:
        path.addRect(mark.box);
        path.moveTo(attachOf(mark));
        path.lineTo(shaftEnd(attachOf(mark), mark.tip, mark.width));
        return path;
    case MarkKind::Rectangle:
    case MarkKind::Image:
        path.addRect(mark.box);
        return path;
    }
    return path;
}

QPolygonF headOf(const Mark &mark)
{
    if (mark.kind == MarkKind::Arrow)
        return arrowHead(mark.tail, mark.tip, mark.width);
    if (mark.kind == MarkKind::Callout)
        return arrowHead(attachOf(mark), mark.tip, mark.width);
    return {};
}

QPointF attachOf(const Mark &mark)
{
    const QRectF &box = mark.box;
    return {std::clamp(mark.tip.x(), box.left(), box.right()), std::clamp(mark.tip.y(), box.top(), box.bottom())};
}

QRectF extentOf(const Mark &mark)
{
    const QRectF area = outlineOf(mark).boundingRect() | headOf(mark).boundingRect();
    const double half = mark.width / 2;
    return area.adjusted(-half, -half, half, half);
}

bool hits(const Mark &mark, QPointF point, double reach)
{
    const QPainterPath outline = outlineOf(mark);
    const bool solid = mark.fill.isValid() || mark.kind == MarkKind::Image || mark.kind == MarkKind::Callout;
    if (solid && outline.contains(point))
        return true;
    QPainterPathStroker stroker;
    stroker.setWidth(mark.width + 2 * reach);
    return stroker.createStroke(outline).contains(point) || headOf(mark).containsPoint(point, Qt::OddEvenFill);
}

std::vector<QPointF> handlesOf(const Mark &mark)
{
    if (mark.kind == MarkKind::Arrow)
        return {mark.tail, mark.tip};
    const QRectF &b = mark.box;
    const QPointF mid = b.center();
    std::vector<QPointF> handles{b.topLeft(),     {mid.x(), b.top()},    b.topRight(),   {b.right(), mid.y()},
                                 b.bottomRight(), {mid.x(), b.bottom()}, b.bottomLeft(), {b.left(), mid.y()}};
    if (mark.kind == MarkKind::Callout)
        handles.push_back(mark.tip);
    return handles;
}

Mark dragged(const Mark &start, int handle, QPointF delta)
{
    Mark mark = start;
    if (handle == kBody) {
        mark.box.translate(delta);
        mark.tail += delta;
        mark.tip += delta;
        return mark;
    }
    if (mark.kind == MarkKind::Arrow) {
        (handle == 0 ? mark.tail : mark.tip) += delta;
        return mark;
    }
    if (handle == int(kSides.size())) {
        mark.tip += delta;
        return mark;
    }
    const Sides &sides = kSides[size_t(handle)];
    const double dx = delta.x();
    const double dy = delta.y();
    mark.box = mark.box.adjusted(sides.left ? dx : 0, sides.top ? dy : 0, sides.right ? dx : 0, sides.bottom ? dy : 0)
                   .normalized();
    return mark;
}
