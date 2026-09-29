#pragma once

#include <QColor>
#include <QDate>
#include <QString>

struct Signature
{
    QString name;
    int font = 0;
    QColor color = QColor(24, 38, 92);
    QString note;
    bool dated = true;
    QDate date = QDate::currentDate();
    double size = 30;
};
