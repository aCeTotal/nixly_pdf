#pragma once

#include "typeset.h"

#include <QTransform>

class QPainter;

// Module space to page points.
QTransform moduleTransform(const Module &module);

// Ink of a run style.
QColor inkOf(const Module &module, int style);

// Paints in page point coordinates.
void paintModule(QPainter &painter, const Module &module, const Layout &layout);
