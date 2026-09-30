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

// Opaque patch hiding page pixels.
struct Cover
{
    fz_quad quad;
    QRgb color;
};

// Removals plus new ink.
struct PageEdit
{
    std::vector<fz_point> removed;
    std::vector<Cover> covers;
    std::vector<TextSpan> spans;
    std::vector<fz_matrix> images;
};

QString rewritePage(fz_context *ctx, pdf_page *page, const PageEdit &edit);

// Placements equal within rounding.
bool samePlacement(fz_matrix a, fz_matrix b);
