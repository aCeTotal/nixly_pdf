#pragma once

#include "moduleset.h"

class Document;

struct Flattening
{
    const Document &doc;
    ModuleSet &modules;
};

// Burns modules into a copy.
QString flatten(fz_context *ctx, pdf_document *copy, const Flattening &source);
