#pragma once

#include <QByteArray>
#include <QColor>
#include <QImage>
#include <QPointF>
#include <QRectF>
#include <QString>

enum class MarkKind { Rectangle, Ellipse, Cloud, Arrow, Callout, Image };

// Shape or picture over page.
struct Mark
{
    int id = 0;
    int page = 0;
    MarkKind kind = MarkKind::Rectangle;
    QRectF box;
    QPointF tail;
    QPointF tip;
    QColor ink = QColor(220, 38, 38);
    QColor fill;
    double width = 2;
    QString text;
    double size = 12;
    QImage image;
    QByteArray encoded;
};
