#pragma once

#include "markset.h"
#include "pdf/context.h"

class Document;

struct Annotating
{
    const Document &doc;
    const MarkSet &marks;
};

// Marks as annotations in copy.
QString annotate(fz_context *ctx, pdf_document *copy, const Annotating &source);
