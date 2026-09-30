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

// Caret spot per text position.
struct Stop
{
    double x;
    int line;
};

// Runs from the first baseline.
struct Layout
{
    std::vector<GlyphRun> runs;
    QRectF bounds;
    std::vector<Stop> stops;
    std::vector<double> baselines;
    double ascent = 0;
    double descent = 0;
};

Layout typeset(const Module &module, FontLibrary &fonts);
