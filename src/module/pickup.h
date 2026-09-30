#pragma once

#include "module.h"
#include "edit/passage.h"
#include "font/fontlibrary.h"

struct Lift
{
    int index;
    Passage passage;
};

struct Lifted
{
    Module module;
    QString error;
};

// Lifts page text into module.
Lifted pickUp(Document &doc, FontLibrary &fonts, const Lift &lift);
