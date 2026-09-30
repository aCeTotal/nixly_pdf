#include "callouteditor.h"

#include "app/theme.h"

#include <QKeyEvent>
#include <algorithm>

namespace {

constexpr double kTextInset = 2;
const QString kCalloutFamily = QStringLiteral("Liberation Sans");

} // namespace

CalloutEditor::CalloutEditor(QWidget *parent) : QPlainTextEdit(parent)
{
    hide();
    setFrameShape(QFrame::NoFrame);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

void CalloutEditor::open(const Mark &mark, double zoom)
{
    current = mark;
    setStyleSheet(QStringLiteral("QPlainTextEdit { background: transparent; color: %1; border: 1.5px dashed %2;"
                                 " border-radius: 4px; selection-background-color: %3; }")
                      .arg(mark.ink.name(), theme::accent.name(),
                           theme::faded(theme::accent, 90).name(QColor::HexArgb)));
    QFont face(kCalloutFamily);
    face.setPixelSize(std::max(1, qRound(mark.size * zoom)));
    setFont(face);
    document()->setDocumentMargin((mark.width / 2 + kTextInset) * zoom);
    setPlainText(mark.text);
    show();
    raise();
    setFocus();
    selectAll();
}

void CalloutEditor::keyPressEvent(QKeyEvent *event)
{
    const bool enter = event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter;
    if (event->key() == Qt::Key_Escape) {
        hide();
        return;
    }
    if (enter && !(event->modifiers() & Qt::ShiftModifier)) {
        commit();
        return;
    }
    QPlainTextEdit::keyPressEvent(event);
}

void CalloutEditor::focusOutEvent(QFocusEvent *event)
{
    QPlainTextEdit::focusOutEvent(event);
    commit();
}

void CalloutEditor::commit()
{
    if (isHidden())
        return;
    hide();
    current.text = toPlainText();
    emit committed(current);
}
