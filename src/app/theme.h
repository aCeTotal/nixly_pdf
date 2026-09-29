#pragma once

#include <QColor>

class QApplication;

namespace theme {

constexpr QColor accent(122, 162, 247);
constexpr QColor text(240, 240, 242);
constexpr QColor muted(139, 143, 154);
constexpr QColor surface(26, 27, 38);
constexpr QColor backdrop(17, 17, 24);
constexpr QColor danger(247, 118, 142);

inline QColor faded(QColor color, int alpha)
{
    color.setAlpha(alpha);
    return color;
}

void apply(QApplication &app);

} // namespace theme
