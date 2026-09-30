#pragma once

#include "module.h"
#include "pdf/content.h"
#include "font/fontlibrary.h"

// One face, style and line.
struct GlyphRun
{
    Face *face;
    QRawFont sized;
    QPointF origin;
    double size;
    int style;
    QList<quint32> gids;
    QList<QPointF> positions;
    std::vector<TjItem> items;
};

// Runs from the first baseline.
struct Layout
{
    std::vector<GlyphRun> runs;
    QRectF bounds;
};

Layout typeset(const Module &module, FontLibrary &fonts);
