#include "moduleeditor.h"

#include "caret.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QKeyEvent>
#include <algorithm>

bool ModuleEditor::motionKey(QKeyEvent *event)
{
    struct Motion
    {
        QKeySequence::StandardKey key;
        Target target;
        QTextCursor::MoveMode mode;
    };
    static constexpr Motion kMotions[] = {
        {QKeySequence::MoveToNextChar, Target::NextChar, QTextCursor::MoveAnchor},
        {QKeySequence::MoveToPreviousChar, Target::PreviousChar, QTextCursor::MoveAnchor},
        {QKeySequence::MoveToNextWord, Target::NextWord, QTextCursor::MoveAnchor},
        {QKeySequence::MoveToPreviousWord, Target::PreviousWord, QTextCursor::MoveAnchor},
        {QKeySequence::MoveToStartOfLine, Target::LineStart, QTextCursor::MoveAnchor},
        {QKeySequence::MoveToEndOfLine, Target::LineEnd, QTextCursor::MoveAnchor},
        {QKeySequence::MoveToNextLine, Target::NextLine, QTextCursor::MoveAnchor},
        {QKeySequence::MoveToPreviousLine, Target::PreviousLine, QTextCursor::MoveAnchor},
        {QKeySequence::MoveToStartOfDocument, Target::Start, QTextCursor::MoveAnchor},
        {QKeySequence::MoveToEndOfDocument, Target::End, QTextCursor::MoveAnchor},
        {QKeySequence::SelectNextChar, Target::NextChar, QTextCursor::KeepAnchor},
        {QKeySequence::SelectPreviousChar, Target::PreviousChar, QTextCursor::KeepAnchor},
        {QKeySequence::SelectNextWord, Target::NextWord, QTextCursor::KeepAnchor},
        {QKeySequence::SelectPreviousWord, Target::PreviousWord, QTextCursor::KeepAnchor},
        {QKeySequence::SelectStartOfLine, Target::LineStart, QTextCursor::KeepAnchor},
        {QKeySequence::SelectEndOfLine, Target::LineEnd, QTextCursor::KeepAnchor},
        {QKeySequence::SelectNextLine, Target::NextLine, QTextCursor::KeepAnchor},
        {QKeySequence::SelectPreviousLine, Target::PreviousLine, QTextCursor::KeepAnchor},
        {QKeySequence::SelectStartOfDocument, Target::Start, QTextCursor::KeepAnchor},
        {QKeySequence::SelectEndOfDocument, Target::End, QTextCursor::KeepAnchor},
    };
    const auto hit = std::ranges::find_if(kMotions, [event](const Motion &m) { return event->matches(m.key); });
    if (hit == std::end(kMotions))
        return false;
    const bool sideways = hit->target == Target::NextChar || hit->target == Target::PreviousChar;
    const bool collapse = sideways && hit->mode == QTextCursor::MoveAnchor && cursor != anchor;
    const qsizetype edge = hit->target == Target::NextChar ? std::max(cursor, anchor) : std::min(cursor, anchor);
    go(collapse ? edge : targetOf(hit->target), hit->mode);
    return true;
}

bool ModuleEditor::commandKey(QKeyEvent *event)
{
    struct Command
    {
        QKeySequence::StandardKey key;
        void (*run)(ModuleEditor &);
    };
    static constexpr Command kCommands[] = {
        {QKeySequence::SelectAll, [](ModuleEditor &e) { e.selectAll(); }},
        {QKeySequence::Copy, [](ModuleEditor &e) { e.copy(); }},
        {QKeySequence::Cut, [](ModuleEditor &e) { e.cut(); }},
        {QKeySequence::Paste, [](ModuleEditor &e) { e.paste(); }},
        {QKeySequence::Undo, [](ModuleEditor &e) { e.undo(); }},
        {QKeySequence::Redo, [](ModuleEditor &e) { e.redo(); }},
        {QKeySequence::Delete, [](ModuleEditor &e) { e.deleteTo(Target::NextChar); }},
        {QKeySequence::DeleteStartOfWord, [](ModuleEditor &e) { e.deleteTo(Target::PreviousWord); }},
        {QKeySequence::DeleteEndOfWord, [](ModuleEditor &e) { e.deleteTo(Target::NextWord); }},
    };
    const auto hit = std::ranges::find_if(kCommands, [event](const Command &c) { return event->matches(c.key); });
    if (hit == std::end(kCommands))
        return false;
    hit->run(*this);
    return true;
}

void ModuleEditor::keyPressEvent(QKeyEvent *event)
{
    if (motionKey(event) || commandKey(event))
        return;
    const bool shifted = event->modifiers() & Qt::ShiftModifier;
    switch (event->key()) {
    case Qt::Key_Escape:
        cancel();
        return;
    case Qt::Key_Backspace:
        deleteTo(Target::PreviousChar);
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        shifted ? type(QStringLiteral("\n")) : commit();
        return;
    default:
        break;
    }
    const QString text = event->text();
    if (text.isEmpty() || !text.front().isPrint()) {
        QWidget::keyPressEvent(event);
        return;
    }
    type(text);
}

qsizetype ModuleEditor::targetOf(Target target)
{
    if (target != Target::NextLine && target != Target::PreviousLine)
        column.reset();
    switch (target) {
    case Target::NextChar:
        return boundary(QTextBoundaryFinder::Grapheme, cursor, 1);
    case Target::PreviousChar:
        return boundary(QTextBoundaryFinder::Grapheme, cursor, -1);
    case Target::NextWord:
        return boundary(QTextBoundaryFinder::Word, cursor, 1);
    case Target::PreviousWord:
        return boundary(QTextBoundaryFinder::Word, cursor, -1);
    case Target::LineStart:
        return lineStart(shape, cursor);
    case Target::LineEnd:
        return lineEnd(shape, cursor);
    case Target::NextLine:
        return stepLine(1);
    case Target::PreviousLine:
        return stepLine(-1);
    case Target::Start:
        return 0;
    case Target::End:
        return current.text.size();
    }
    return cursor;
}

// Keeps the column across lines.
qsizetype ModuleEditor::stepLine(int lines)
{
    const Stop &stop = shape.stops[size_t(cursor)];
    column = column.value_or(stop.x);
    const int line = stop.line + lines;
    if (line < 0)
        return 0;
    if (line >= int(shape.baselines.size()))
        return current.text.size();
    return positionOnLine(shape, line, *column);
}

qsizetype ModuleEditor::boundary(QTextBoundaryFinder::BoundaryType type, qsizetype from, int direction) const
{
    QTextBoundaryFinder finder(type, current.text);
    finder.setPosition(from);
    const qsizetype found = direction > 0 ? finder.toNextBoundary() : finder.toPreviousBoundary();
    if (found >= 0)
        return found;
    return direction > 0 ? current.text.size() : 0;
}

void ModuleEditor::deleteTo(Target target)
{
    remember(Change::Deleting);
    const qsizetype other = cursor != anchor ? anchor : targetOf(target);
    erase(std::min(cursor, other), std::max(cursor, other));
}

void ModuleEditor::selectAll()
{
    anchor = 0;
    go(current.text.size(), QTextCursor::KeepAnchor);
}

void ModuleEditor::copy()
{
    if (cursor == anchor)
        return;
    const qsizetype from = std::min(cursor, anchor);
    QGuiApplication::clipboard()->setText(current.text.mid(from, std::max(cursor, anchor) - from));
}

void ModuleEditor::cut()
{
    if (cursor == anchor)
        return;
    copy();
    deleteTo(Target::NextChar);
}

void ModuleEditor::paste()
{
    const QString text = QGuiApplication::clipboard()->text().remove('\r');
    if (text.isEmpty())
        return;
    remember(Change::None);
    replace(text);
}

void ModuleEditor::undo()
{
    restore(undos, redos);
}

void ModuleEditor::redo()
{
    restore(redos, undos);
}
