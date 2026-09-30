#pragma once

#include "context.h"

#include <QRectF>
#include <vector>

// Filled areas on a page.
void traceBackdrops(fz_context *ctx, fz_page *page, std::vector<QRectF> &boxes);
