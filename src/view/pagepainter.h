#pragma once

#include <QRectF>

class QPainter;
class Renderer;

// Drawing inputs for one page.
struct PageFrame
{
    int id;
    QRectF rect;
    QRectF clip;
    double zoom;
    double dpr;
    int scale;
    int fallback;
};

// Shadow, preview, then sharp tiles.
void paintPage(QPainter &painter, Renderer &renderer, const PageFrame &frame);

// Queues tiles without painting.
void prefetchPage(Renderer &renderer, const PageFrame &frame);
