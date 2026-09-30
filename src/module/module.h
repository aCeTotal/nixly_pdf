#pragma once

#include <QColor>
#include <QDate>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <vector>

enum class ModuleKind { Text, Date, Signature };

// Page font or family.
struct FontChoice
{
    int pageFont = 0;
    QString family;
    bool bold = false;
    bool italic = false;

    bool operator==(const FontChoice &) const = default;
};

// Stretch styled unlike body.
struct Span
{
    int start = 0;
    int length = 0;
    FontChoice font;
    double scale = 1;
    QColor color;

    bool operator==(const Span &) const = default;
};

// Movable text over a page.
struct Module
{
    int id = 0;
    int page = 0;
    ModuleKind kind = ModuleKind::Text;
    QPointF anchor;
    double angle = 0;
    double stretch = 1;
    double width = 0;
    double leading = 0;
    QString text;
    std::vector<Span> spans;
    FontChoice font;
    int originalFont = 0;
    double size = 12;
    QColor color = Qt::black;
    QRectF cover;
    QColor coverColor = Qt::white;
    QDate date;
};
