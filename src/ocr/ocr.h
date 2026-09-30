#pragma once

#include <QPointF>
#include <QRectF>
#include <QRgb>
#include <QString>
#include <vector>

class Document;

// Recognised paragraph in page points.
struct OcrBlock
{
    QString text;
    QRectF box;
    QPointF baseline;
    double angle;
    double leading;
    double size;
    QRgb ink;
    QRgb paper;
};

struct OcrResult
{
    std::vector<OcrBlock> blocks;
    QString error;
};

// Safe on worker threads.
OcrResult recognize(Document &doc, int index);
