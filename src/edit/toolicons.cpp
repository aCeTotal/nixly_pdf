#include "toolicons.h"

#include "app/theme.h"

#include <QPainter>
#include <QPainterPath>
#include <array>

namespace {

constexpr int kGrid = 24;
constexpr int kOversample = 3;
constexpr double kStroke = 1.7;
constexpr int kDisabledAlpha = 90;

using Draw = void (*)(QPainterPath &);

void page(QPainterPath &p, double left)
{
    const double right = left + 12.5;
    p.moveTo(left, 3.5);
    p.lineTo(right - 4.5, 3.5);
    p.lineTo(right, 8);
    p.lineTo(right, 20.5);
    p.lineTo(left, 20.5);
    p.closeSubpath();
    p.moveTo(right - 4.5, 3.5);
    p.lineTo(right - 4.5, 8);
    p.lineTo(right, 8);
}

void line(QPainterPath &p, QPointF from, QPointF to)
{
    p.moveTo(from);
    p.lineTo(to);
}

constexpr std::array<Draw, 12> kDrawings{
    +[](QPainterPath &p) {
        line(p, {6, 6}, {18, 6});
        line(p, {12, 6}, {12, 19});
        line(p, {9.5, 19}, {14.5, 19});
    },
    +[](QPainterPath &p) {
        p.addRoundedRect(4, 6, 16, 14, 2.5, 2.5);
        line(p, {4, 10.5}, {20, 10.5});
        line(p, {8.5, 4}, {8.5, 8});
        line(p, {15.5, 4}, {15.5, 8});
        line(p, {8, 15}, {9.5, 15});
        line(p, {11.25, 15}, {12.75, 15});
        line(p, {14.5, 15}, {16, 15});
    },
    +[](QPainterPath &p) {
        p.addRoundedRect(3.5, 5, 17, 14, 2.5, 2.5);
        p.moveTo(4.5, 17);
        p.lineTo(9.5, 11.5);
        p.lineTo(13.5, 15.5);
        p.lineTo(15.5, 13.5);
        p.lineTo(19.5, 17);
        p.addEllipse(QPointF(15.5, 9), 1.4, 1.4);
    },
    +[](QPainterPath &p) {
        line(p, {5, 19}, {18.5, 5.5});
        p.moveTo(11, 5.5);
        p.lineTo(18.5, 5.5);
        p.lineTo(18.5, 13);
    },
    +[](QPainterPath &p) {
        p.addRoundedRect(9, 4, 12, 9, 2.5, 2.5);
        line(p, {11.5, 7.5}, {18.5, 7.5});
        line(p, {11.5, 10}, {16, 10});
        line(p, {11, 13}, {4.5, 19.5});
        p.moveTo(4.5, 15.5);
        p.lineTo(4.5, 19.5);
        p.lineTo(8.5, 19.5);
    },
    +[](QPainterPath &p) { p.addRect(4, 6, 16, 12); },
    +[](QPainterPath &p) { p.addEllipse(3.5, 6, 17, 12); },
    +[](QPainterPath &p) {
        QPainterPath left, middle, right, base;
        left.addEllipse(3.5, 10.5, 8, 7.5);
        middle.addEllipse(7.5, 6, 9.5, 9.5);
        right.addEllipse(13, 9.5, 7.5, 8);
        base.addRect(7.5, 13, 9.5, 5);
        p.addPath(left.united(middle).united(right).united(base).simplified());
    },
    +[](QPainterPath &p) {
        p.moveTo(4, 9);
        p.lineTo(4, 4);
        p.lineTo(9, 4);
        p.moveTo(15, 4);
        p.lineTo(20, 4);
        p.lineTo(20, 9);
        p.moveTo(20, 15);
        p.lineTo(20, 20);
        p.lineTo(15, 20);
        p.moveTo(9, 20);
        p.lineTo(4, 20);
        p.lineTo(4, 15);
        line(p, {8, 9.5}, {16, 9.5});
        line(p, {8, 12.5}, {16, 12.5});
        line(p, {8, 15.5}, {13, 15.5});
    },
    +[](QPainterPath &p) {
        page(p, 6);
        line(p, {12.25, 11}, {12.25, 17});
        line(p, {9.25, 14}, {15.25, 14});
    },
    +[](QPainterPath &p) {
        page(p, 8.5);
        line(p, {2.5, 14}, {12.5, 14});
        p.moveTo(9.5, 11);
        p.lineTo(12.5, 14);
        p.lineTo(9.5, 17);
    },
    +[](QPainterPath &p) {
        line(p, {4.5, 7}, {19.5, 7});
        p.moveTo(9.5, 7);
        p.lineTo(10, 4.5);
        p.lineTo(14, 4.5);
        p.lineTo(14.5, 7);
        p.moveTo(6.5, 7);
        p.lineTo(7.5, 20);
        p.lineTo(16.5, 20);
        p.lineTo(17.5, 7);
        line(p, {10.25, 10.5}, {10.5, 16.5});
        line(p, {13.75, 10.5}, {13.5, 16.5});
    },
};

QPixmap drawn(const QPainterPath &path, const QColor &color)
{
    QPixmap pixmap(kGrid * kOversample, kGrid * kOversample);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(kOversample, kOversample);
    painter.setPen(QPen(color, kStroke, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPath(path);
    return pixmap;
}

} // namespace

QIcon toolIcon(Tool tool)
{
    QPainterPath path;
    kDrawings[size_t(tool)](path);
    QIcon icon;
    icon.addPixmap(drawn(path, theme::text), QIcon::Normal, QIcon::Off);
    icon.addPixmap(drawn(path, theme::accent), QIcon::Normal, QIcon::On);
    icon.addPixmap(drawn(path, theme::faded(theme::text, kDisabledAlpha)), QIcon::Disabled, QIcon::Off);
    return icon;
}
