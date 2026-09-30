#pragma once

#include "typeset.h"

#include <QLineF>

// Caret in module space.
QLineF caretAt(const Layout &layout, qsizetype position);

// Position nearest a module point.
qsizetype positionAt(const Layout &layout, QPointF point);

// Line position nearest x.
qsizetype positionOnLine(const Layout &layout, int line, double x);

qsizetype lineStart(const Layout &layout, qsizetype position);
qsizetype lineEnd(const Layout &layout, qsizetype position);

// Highlight boxes for a range.
std::vector<QRectF> selectionBoxes(const Layout &layout, qsizetype from, qsizetype to);
