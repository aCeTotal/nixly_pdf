#pragma once

#include "module.h"

// Inserts in the preceding style.
void insertText(Module &module, qsizetype at, const QString &text);

// Removes a range, spans shrink.
void removeText(Module &module, qsizetype from, qsizetype to);
