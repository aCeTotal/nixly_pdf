#pragma once

#include "mark.h"

#include <QFrame>

class QDoubleSpinBox;
class QToolButton;

// Style controls for a mark.
class MarkBar : public QFrame
{
    Q_OBJECT

public:
    explicit MarkBar(QWidget *parent);
    void present(const Mark &mark);

signals:
    void changed(const Mark &mark);
    void removeRequested(int id);

private:
    void pickInk();
    void pickFill();

    Mark current;
    QToolButton *ink;
    QToolButton *fill;
    QDoubleSpinBox *width;
    QDoubleSpinBox *size;
};
