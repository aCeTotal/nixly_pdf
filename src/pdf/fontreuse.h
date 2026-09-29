#pragma once

#include "content.h"

#include <QString>

// Consecutive glyphs drawn by one font.
struct Segment
{
    pdf_obj *font;
    int codeBytes;
    std::vector<TjItem> items;
    float advance;
};

struct Encoding
{
    std::vector<Segment> segments;
    QString substitute;
    bool embedded = false;
};

// Page fonts first, else closest.
QString encodeText(fz_context *ctx, pdf_page *page, fz_font *font, const QString &text, Encoding *out);

void dropEncoding(fz_context *ctx, Encoding &encoding);
