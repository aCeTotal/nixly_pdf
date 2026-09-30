#include "caret.h"

#include <algorithm>
#include <cmath>

namespace {

int lineNear(const Layout &layout, double y)
{
    const auto distance = [&layout, y](double base) {
        const double top = base - layout.ascent;
        const double bottom = base + layout.descent;
        return y < top ? top - y : y > bottom ? y - bottom : 0.0;
    };
    const auto nearest = std::ranges::min_element(layout.baselines, {}, distance);
    return int(nearest - layout.baselines.begin());
}

double lineRight(const Layout &layout, int line)
{
    double right = 0;
    for (const Stop &stop : layout.stops) {
        if (stop.line == line)
            right = std::max(right, stop.x);
    }
    return right;
}

} // namespace

QLineF caretAt(const Layout &layout, qsizetype position)
{
    const Stop &stop = layout.stops[size_t(position)];
    const double base = layout.baselines[size_t(stop.line)];
    return QLineF(stop.x, base - layout.ascent, stop.x, base + layout.descent);
}

qsizetype positionAt(const Layout &layout, QPointF point)
{
    return positionOnLine(layout, lineNear(layout, point.y()), point.x());
}

qsizetype positionOnLine(const Layout &layout, int line, double x)
{
    qsizetype best = -1;
    for (qsizetype i = 0; i < qsizetype(layout.stops.size()); ++i) {
        const Stop &stop = layout.stops[size_t(i)];
        const bool closer = best < 0 || std::fabs(stop.x - x) < std::fabs(layout.stops[size_t(best)].x - x);
        if (stop.line == line && closer)
            best = i;
    }
    return std::max<qsizetype>(best, 0);
}

qsizetype lineStart(const Layout &layout, qsizetype position)
{
    const int line = layout.stops[size_t(position)].line;
    while (position > 0 && layout.stops[size_t(position - 1)].line == line)
        --position;
    return position;
}

qsizetype lineEnd(const Layout &layout, qsizetype position)
{
    const int line = layout.stops[size_t(position)].line;
    const qsizetype last = qsizetype(layout.stops.size()) - 1;
    while (position < last && layout.stops[size_t(position + 1)].line == line)
        ++position;
    return position;
}

std::vector<QRectF> selectionBoxes(const Layout &layout, qsizetype from, qsizetype to)
{
    std::vector<QRectF> boxes;
    const Stop &first = layout.stops[size_t(std::min(from, to))];
    const Stop &last = layout.stops[size_t(std::max(from, to))];
    for (int line = first.line; line <= last.line; ++line) {
        const double left = line == first.line ? first.x : 0;
        const double right = line == last.line ? last.x : lineRight(layout, line);
        const double base = layout.baselines[size_t(line)];
        boxes.emplace_back(QPointF(left, base - layout.ascent), QPointF(right, base + layout.descent));
    }
    return boxes;
}
