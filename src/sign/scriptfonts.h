#pragma once

#include <QString>
#include <vector>

struct ScriptFont
{
    QString path;
    QString family;
};

// Handwriting fonts offered for signatures.
const std::vector<ScriptFont> &scriptFonts();

// Sans font for labels.
const QString &labelFont();
