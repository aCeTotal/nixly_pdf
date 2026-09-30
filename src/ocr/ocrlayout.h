#pragma once

#include "ocr.h"

// Recognised word in image pixels.
struct OcrWord
{
    QString text;
    QRectF box;
    QPointF baseline;
    double angle;
    int line;
    int paragraph;
    int cell = -1;
};

// RGB raster mapped to points.
struct OcrImage
{
    const unsigned char *samples;
    int width;
    int height;
    int stride;
    QPointF origin;
    double scale;

    QRectF toPoints(const QRectF &pixels) const { return {pixels.topLeft() * scale + origin, pixels.size() * scale}; }
};

// Words into columns and paragraphs.
std::vector<OcrBlock> layoutBlocks(std::vector<OcrWord> words, const OcrImage &image);
