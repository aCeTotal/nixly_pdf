#include "cells.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace {

constexpr double kRule = 2.5;
constexpr double kInside = 1;
constexpr double kOpen = std::numeric_limits<double>::infinity();

struct Rules
{
    std::vector<QRectF> across;
    std::vector<QRectF> down;
};

Rules rulesOf(const std::vector<QRectF> &boxes)
{
    Rules rules;
    for (const QRectF &box : boxes) {
        if (box.height() <= kRule && box.width() > box.height())
            rules.across.push_back(box);
        else if (box.width() <= kRule && box.height() > box.width())
            rules.down.push_back(box);
    }
    return rules;
}

// Nearest rule on each side.
std::optional<QRectF> cellAround(const Rules &rules, const QRectF &line)
{
    const QPointF mid = line.center();
    double left = -kOpen, right = kOpen, top = -kOpen, bottom = kOpen;
    for (const QRectF &rule : rules.down) {
        if (rule.top() > mid.y() || rule.bottom() < mid.y())
            continue;
        if (rule.right() <= line.left() + kInside)
            left = std::max(left, rule.right());
        if (rule.left() >= line.right() - kInside)
            right = std::min(right, rule.left());
    }
    for (const QRectF &rule : rules.across) {
        if (rule.left() > mid.x() || rule.right() < mid.x())
            continue;
        if (rule.bottom() <= line.top() + kInside)
            top = std::max(top, rule.bottom());
        if (rule.top() >= line.bottom() - kInside)
            bottom = std::min(bottom, rule.top());
    }
    if (std::isinf(left) || std::isinf(right) || std::isinf(top) || std::isinf(bottom))
        return std::nullopt;
    return QRectF(QPointF(left, top), QPointF(right, bottom));
}

// Same row, different column.
bool beside(const QRectF &a, const QRectF &b)
{
    const bool row = std::min(a.bottom(), b.bottom()) > std::max(a.top(), b.top());
    return row && (a.right() <= b.left() + kInside || b.right() <= a.left() + kInside);
}

} // namespace

std::vector<QRectF> tableCells(const std::vector<QRectF> &texts, const std::vector<QRectF> &boxes)
{
    const Rules rules = rulesOf(boxes);
    if (rules.across.empty() || rules.down.empty())
        return {};
    std::vector<QRectF> framed;
    for (const QRectF &text : texts) {
        const std::optional<QRectF> cell = cellAround(rules, text);
        if (cell && std::ranges::find(framed, *cell) == framed.end())
            framed.push_back(*cell);
    }
    std::vector<QRectF> cells;
    for (const QRectF &cell : framed) {
        if (std::ranges::any_of(framed, [&cell](const QRectF &other) { return beside(cell, other); }))
            cells.push_back(cell);
    }
    return cells;
}
