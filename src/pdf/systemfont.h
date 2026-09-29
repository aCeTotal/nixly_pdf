#pragma once

#include <QByteArray>
#include <QString>

struct FontQuery
{
    QString family;
    QString generic;
    bool bold = false;
    bool italic = false;
    QString text;
};

struct FontFile
{
    QByteArray path;
    int index;
    QString family;
};

// Installed font covering the text.
FontFile matchFont(const FontQuery &query);
