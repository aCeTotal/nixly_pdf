#pragma once

#include "signature.h"

#include <QDialog>

class QButtonGroup;
class QGridLayout;
class QHBoxLayout;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSlider;
class SignaturePreview;

// Composes a signature.
class SignDialog : public QDialog
{
public:
    SignDialog(const Signature &initial, QWidget *parent);
    Signature signature() const { return chosen; }

private:
    QGridLayout *buildStyles();
    QHBoxLayout *buildInks();
    QPushButton *addSwatch(QHBoxLayout *row);
    QSlider *buildSize();
    QHBoxLayout *buildDate();
    QPlainTextEdit *buildNote();
    QHBoxLayout *buildButtons();
    void refresh();

    Signature chosen;
    SignaturePreview *preview;
    QLineEdit *name;
    QButtonGroup *styles;
    QButtonGroup *inks;
    QPushButton *place;
};
