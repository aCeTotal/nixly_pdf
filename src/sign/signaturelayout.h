#pragma once

#include "pdf/stamp.h"
#include "signature.h"

#include <QRawFont>
#include <QSizeF>
#include <vector>

class QPainter;

// Shaped signature, centred, in points.
class SignatureLayout
{
public:
    explicit SignatureLayout(const Signature &signature);

    QSizeF size() const { return extent; }
    void paint(QPainter &painter) const;
    std::vector<StampLine> stampLines(QPointF centre) const;

private:
    struct Line
    {
        QString path;
        QRawFont font;
        QList<quint32> glyphs;
        QList<QPointF> positions;
        std::vector<TjItem> items;
        QPointF baseline;
    };

    void add(const QString &path, const QString &text, double size);

    std::vector<Line> lines;
    QSizeF extent;
    QColor color;
};
