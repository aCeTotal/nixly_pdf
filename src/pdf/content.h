#pragma once

#include "context.h"

#include <QRgb>
#include <vector>

// Glyph code or bare shift.
struct TjItem
{
    int code;
    float shift;
};

struct TextSpan
{
    pdf_obj *font;
    int codeBytes;
    fz_matrix matrix;
    QRgb color;
    std::vector<TjItem> items;
};

// Drops glyphs, then appends spans.
QString rewritePage(fz_context *ctx, pdf_document *doc, pdf_page *page, const std::vector<fz_point> &removed,
                    const std::vector<TextSpan> &spans);
