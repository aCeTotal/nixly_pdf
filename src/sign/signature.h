#pragma once

#include "module/module.h"

struct Signature
{
    QString name;
    int font = 0;
    QColor color = QColor(24, 38, 92);
    double size = 30;
};

// Placeable module drawing the signature.
Module signatureModule(const Signature &signature);
