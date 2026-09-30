#pragma once

#include "textpage.h"

#include <QString>

// Runs lifted as one module.
struct Passage
{
    std::vector<TextRun> runs;
    // Text before each run.
    std::vector<QString> joints;
    std::vector<QRectF> outlines;
    double leading = 0;
};

// Paragraph or panel around line.
Passage passageAt(const TextPage &page, size_t line);
