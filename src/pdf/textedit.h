#pragma once

#include "textrun.h"

struct EditOutcome
{
    QString error;
    QString substitute;
};

// Replaces a run, keeping font.
EditOutcome replaceRun(Document &doc, int index, const TextRun &target, const QString &text);
