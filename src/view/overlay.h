#pragma once

#include "smoothvalue.h"

#include <QRectF>
#include <QString>

class QPainter;

// Scroll position for overlay.
struct ScrollState
{
    QRectF viewport;
    double offset;
    double extent;
};

// Fading scroll thumb, page badge.
class ScrollOverlay
{
public:
    static constexpr double kGrip = 14;

    void wake();
    bool advance(double seconds);
    void paint(QPainter &painter, const ScrollState &state, const QString &badge) const;
    QRectF thumb(const ScrollState &state) const;

private:
    double hold = 0;
    SmoothValue opacity;
};
