#pragma once

#include "pdf/textrun.h"

// Runs along one baseline.
struct Line
{
    size_t first;
    size_t last;
    QRectF box;
    int panel = -1;
};

// Page runs in lines, panels.
struct TextPage
{
    std::vector<TextRun> runs;
    std::vector<Line> lines;
    std::vector<QRectF> panels;
    std::vector<Picture> pictures;
};

TextPage arrange(PageText text);
