#pragma once

#include "mark.h"

class QPainter;

// Paints in page point coordinates.
void paintMark(QPainter &painter, const Mark &mark);
