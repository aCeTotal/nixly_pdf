#pragma once

#include <QFrame>

class QPropertyAnimation;

// Sliding page tools while editing.
class EditBar : public QFrame
{
    Q_OBJECT

public:
    explicit EditBar(QWidget *parent = nullptr);
    void reveal();
    void conceal();

signals:
    void addBlank();
    void insertFile();
    void deletePage();

private:
    void slideTo(int height);

    QPropertyAnimation *slide;
};
