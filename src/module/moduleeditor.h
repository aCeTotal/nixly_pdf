#pragma once

#include "typeset.h"

#include <QPlainTextEdit>

// Inline module text editor.
class ModuleEditor : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit ModuleEditor(QWidget *parent);
    void open(const Module &module, FontLibrary &fonts, double zoom);
    void place(QPointF anchor);
    int module() const { return current.id; }

signals:
    void committed(const Module &module);
    void cancelled();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    void dress(const Layout &shape);
    void commit();
    void cancel();
    void fit();

    Module current;
    FontLibrary *library = nullptr;
    double scale = 1;
};
