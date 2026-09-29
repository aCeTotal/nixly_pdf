#pragma once

#include <QString>

extern "C" {
#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
}

// Context for the GUI thread.
fz_context *mainContext();

// Per-thread context clone.
fz_context *threadContext();

// Pending MuPDF error text.
QString takeError(fz_context *ctx);

// Runs MuPDF code, returns error.
template <typename Body>
QString attempt(fz_context *ctx, Body body)
{
    fz_try(ctx)
        body();
    fz_catch(ctx)
        return takeError(ctx);
    return {};
}
