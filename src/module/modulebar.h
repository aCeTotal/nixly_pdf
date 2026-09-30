#pragma once

#include "module.h"

#include <QFrame>

class QComboBox;
class QDateEdit;
class QDoubleSpinBox;
class QToolButton;

// Style controls for selection.
class ModuleBar : public QFrame
{
    Q_OBJECT

public:
    explicit ModuleBar(QWidget *parent);
    void present(const Module &module, const QString &originalFamily);

signals:
    void changed(const Module &module);
    void removeRequested(int id);

private:
    QToolButton *toggle(const QString &text, const QString &tip);
    void chooseFamily(int row);
    void setStyle();
    void pickColour();

    Module current;
    QString original;
    QComboBox *family;
    QDoubleSpinBox *size;
    QToolButton *bold;
    QToolButton *italic;
    QToolButton *ink;
    QDateEdit *date;
};
