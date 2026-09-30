#pragma once

#include "mark/mark.h"

#include <QFrame>

class QButtonGroup;
class QPropertyAnimation;

// Sliding page tools while editing.
class EditBar : public QFrame
{
    Q_OBJECT

public:
    explicit EditBar(QWidget *parent = nullptr);
    void reveal();
    void conceal();
    // Unchecks the armed placing tool.
    void release();

signals:
    void addText();
    void addDate();
    void drawMark(MarkKind kind);
    void recognize();
    void addBlank();
    void insertFile();
    void deletePage();

private:
    void slideTo(int height);

    QPropertyAnimation *slide;
    QButtonGroup *placing;
};
