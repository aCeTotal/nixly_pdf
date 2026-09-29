#pragma once

#include "context.h"

#include <QPointF>
#include <QRectF>
#include <QRgb>
#include <QString>
#include <vector>

class Document;

// Line stretch sharing one style.
struct TextRun
{
    QString text;
    QRectF box;
    QPointF direction;
    float size;
    QRgb color;
    QString font;
    bool bold;
    bool italic;
    bool serif;
    std::vector<QPointF> glyphs;
};

// Run plus its live font.
struct LiveRun
{
    TextRun run;
    fz_font *font;
};

fz_stext_page *extractText(fz_context *ctx, fz_page *page);
std::vector<LiveRun> collectRuns(fz_context *ctx, fz_stext_page *text);
std::vector<TextRun> pageRuns(Document &doc, int index);
