#pragma once

#include <QString>

// Name without "ABCDEF+" subset tag.
const char *baseFontName(const char *name);

// Family name behind PDF font.
QString familyOf(const QString &name);

// Weight words like Bold, Halvfet.
bool namesBold(const QString &name);
