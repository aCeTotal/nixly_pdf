#pragma once

#include "typeset.h"

#include <QTextBoundaryFinder>
#include <QTextCursor>
#include <QTimer>
#include <QTransform>
#include <QWidget>
#include <optional>

// Edits a module as rendered.
class ModuleEditor : public QWidget
{
    Q_OBJECT

public:
    explicit ModuleEditor(QWidget *parent);
    // Opens, caret nearest the point.
    void open(const Module &module, FontLibrary &fonts, std::optional<QPointF> at);
    void place(const QTransform &moduleToView);
    int module() const { return current.id; }

signals:
    void edited(const Module &module);
    void committed(const Module &module);
    void cancelled(const Module &original);

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void inputMethodEvent(QInputMethodEvent *event) override;
    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    enum class Target { NextChar, PreviousChar, NextWord, PreviousWord, LineStart, LineEnd, NextLine, PreviousLine,
                        Start, End };
    enum class Change { None, Typing, Deleting };

    // Undo point: text, spans, caret.
    struct Snapshot
    {
        QString text;
        std::vector<Span> spans;
        qsizetype cursor;
    };

    bool motionKey(QKeyEvent *event);
    bool commandKey(QKeyEvent *event);
    qsizetype targetOf(Target target);
    qsizetype stepLine(int lines);
    qsizetype boundary(QTextBoundaryFinder::BoundaryType type, qsizetype from, int direction) const;
    void go(qsizetype position, QTextCursor::MoveMode mode);
    void type(const QString &text);
    void replace(const QString &text);
    void erase(qsizetype from, qsizetype to);
    void deleteTo(Target target);
    void remember(Change change);
    void restore(std::vector<Snapshot> &from, std::vector<Snapshot> &to);
    void relayout();
    QPointF toModule(QPointF local) const;
    void selectAll();
    void copy();
    void cut();
    void paste();
    void undo();
    void redo();
    void commit();
    void cancel();

    Module current;
    Module original;
    Layout shape;
    FontLibrary *library = nullptr;
    QTransform toView;
    qsizetype cursor = 0;
    qsizetype anchor = 0;
    std::optional<double> column;
    Change last = Change::None;
    std::vector<Snapshot> undos;
    std::vector<Snapshot> redos;
    QTimer blink;
    bool lit = true;
};
