#pragma once

#include <QColor>
#include <QString>

class QApplication;
class QAbstractButton;
class QLabel;
class QWidget;

namespace theme {

// Material dark error, wallpaper independent.
constexpr QColor danger(255, 180, 171);

QColor accent();
QColor text();
QColor muted();
QColor surface();
QColor backdrop();

inline QColor faded(QColor color, int alpha)
{
    color.setAlpha(alpha);
    return color;
}

// Follows the platform palette live.
void apply(QApplication &app);

// Small uppercase section label.
QLabel *caption(const QString &text);

// Round colour swatch button.
void paintInk(QAbstractButton *button, const QColor &colour);

// Swatch struck through, no colour.
void paintHollow(QAbstractButton *button);

// Shadow under floating panel.
void lift(QWidget *panel);

} // namespace theme
