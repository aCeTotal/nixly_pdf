#pragma once

#include <QRectF>
#include <vector>

// Cells framed by ruling lines.
std::vector<QRectF> tableCells(const std::vector<QRectF> &texts, const std::vector<QRectF> &boxes);
