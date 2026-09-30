#pragma once

#include "mark.h"

#include <QPlainTextEdit>

// Inline callout text editor.
class CalloutEditor : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit CalloutEditor(QWidget *parent);
    void open(const Mark &mark, double zoom);
    int mark() const { return current.id; }

signals:
    void committed(const Mark &mark);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    void commit();

    Mark current;
};
