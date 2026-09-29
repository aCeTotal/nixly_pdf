#pragma once

#include "content.h"

#include <QByteArray>
#include <QPointF>
#include <QString>

class Document;

// Shaped stamp line, page space.
struct StampLine
{
    QByteArray fontPath;
    float size;
    QPointF baseline;
    QRgb color;
    std::vector<TjItem> glyphs;
};

QString placeStamp(Document &doc, int index, const std::vector<StampLine> &lines);
