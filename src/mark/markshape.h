#pragma once

#include "mark.h"

#include <QPainterPath>
#include <QPolygonF>
#include <vector>

// Handle that moves everything.
constexpr int kBody = -1;

// Scallop size of saved clouds.
constexpr float kCloudIntensity = 2;

// Stroke path, without arrowheads.
QPainterPath outlineOf(const Mark &mark);
// Filled arrowhead, empty for shapes.
QPolygonF headOf(const Mark &mark);

// Callout leader start.
QPointF attachOf(const Mark &mark);

// Page area the mark covers.
QRectF extentOf(const Mark &mark);
bool hits(const Mark &mark, QPointF point, double reach);
std::vector<QPointF> handlesOf(const Mark &mark);

// Mark after dragging a handle.
Mark dragged(const Mark &start, int handle, QPointF delta);
