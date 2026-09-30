#include "moduleeditor.h"

#include "app/theme.h"
#include "caret.h"
#include "textedit.h"

#include <QFocusEvent>
#include <QInputMethodEvent>
#include <QMouseEvent>
#include <QPainter>
#include <algorithm>

namespace {

constexpr int kBlinkMs = 530;
constexpr double kMargin = 5;
constexpr double kCaretWidth = 1.5;
constexpr double kFrameWidth = 1.5;
constexpr double kRadius = 4;
constexpr int kSelectionAlpha = 80;

} // namespace

ModuleEditor::ModuleEditor(QWidget *parent) : QWidget(parent)
{
    hide();
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_InputMethodEnabled);
    setCursor(Qt::IBeamCursor);
    blink.setInterval(kBlinkMs);
    connect(&blink, &QTimer::timeout, this, [this] {
        lit = !lit;
        update();
    });
}

void ModuleEditor::open(const Module &module, FontLibrary &fonts, std::optional<QPointF> at)
{
    current = module;
    original = module;
    library = &fonts;
    shape = typeset(module, fonts);
    undos.clear();
    redos.clear();
    last = Change::None;
    column.reset();
    cursor = at ? positionAt(shape, *at) : module.text.size();
    anchor = cursor;
    lit = true;
    blink.start();
    show();
    raise();
    setFocus();
}

void ModuleEditor::place(const QTransform &moduleToView)
{
    toView = moduleToView;
    const QRectF area = toView.mapRect(shape.bounds).adjusted(-kMargin, -kMargin, kMargin, kMargin);
    setGeometry(area.toAlignedRect());
    update();
}

void ModuleEditor::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QTransform local = toView * QTransform::fromTranslate(-x(), -y());
    painter.setPen(QPen(theme::accent, kFrameWidth, Qt::DashLine));
    painter.drawRoundedRect(QRectF(rect()).adjusted(1, 1, -1, -1), kRadius, kRadius);
    painter.setPen(Qt::NoPen);
    painter.setBrush(theme::faded(theme::accent, kSelectionAlpha));
    for (const QRectF &box : selectionBoxes(shape, anchor, cursor))
        painter.drawPolygon(local.map(QPolygonF(box)));
    if (!lit || !hasFocus())
        return;
    painter.setPen(QPen(theme::accent, kCaretWidth));
    painter.drawLine(local.map(caretAt(shape, cursor)));
}

QPointF ModuleEditor::toModule(QPointF local) const
{
    return toView.inverted().map(local + QPointF(pos()));
}

void ModuleEditor::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;
    const bool extend = event->modifiers() & Qt::ShiftModifier;
    go(positionAt(shape, toModule(event->position())), extend ? QTextCursor::KeepAnchor : QTextCursor::MoveAnchor);
}

void ModuleEditor::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton)
        go(positionAt(shape, toModule(event->position())), QTextCursor::KeepAnchor);
}

void ModuleEditor::mouseDoubleClickEvent(QMouseEvent *event)
{
    const qsizetype at = positionAt(shape, toModule(event->position()));
    const qsizetype start = boundary(QTextBoundaryFinder::Word, std::min(at + 1, current.text.size()), -1);
    go(start, QTextCursor::MoveAnchor);
    go(boundary(QTextBoundaryFinder::Word, start, 1), QTextCursor::KeepAnchor);
}

void ModuleEditor::inputMethodEvent(QInputMethodEvent *event)
{
    if (!event->commitString().isEmpty())
        type(event->commitString());
    event->accept();
}

QVariant ModuleEditor::inputMethodQuery(Qt::InputMethodQuery query) const
{
    const QTransform local = toView * QTransform::fromTranslate(-x(), -y());
    switch (query) {
    case Qt::ImEnabled:
        return true;
    case Qt::ImCursorRectangle:
        return local.map(QPolygonF({caretAt(shape, cursor).p1(), caretAt(shape, cursor).p2()})).boundingRect();
    case Qt::ImCursorPosition:
        return int(cursor);
    case Qt::ImAnchorPosition:
        return int(anchor);
    case Qt::ImSurroundingText:
        return current.text;
    case Qt::ImHints:
        return int(Qt::ImhMultiLine);
    default:
        return QWidget::inputMethodQuery(query);
    }
}

void ModuleEditor::focusOutEvent(QFocusEvent *event)
{
    QWidget::focusOutEvent(event);
    const bool passing = event->reason() == Qt::PopupFocusReason || event->reason() == Qt::ActiveWindowFocusReason;
    if (!passing)
        commit();
}

void ModuleEditor::go(qsizetype position, QTextCursor::MoveMode mode)
{
    cursor = std::clamp<qsizetype>(position, 0, current.text.size());
    anchor = mode == QTextCursor::MoveAnchor ? cursor : anchor;
    last = Change::None;
    lit = true;
    blink.start();
    update();
}

void ModuleEditor::type(const QString &text)
{
    remember(Change::Typing);
    replace(text);
}

void ModuleEditor::replace(const QString &text)
{
    const qsizetype from = std::min(cursor, anchor);
    removeText(current, from, std::max(cursor, anchor));
    insertText(current, from, text);
    cursor = from + text.size();
    anchor = cursor;
    column.reset();
    relayout();
}

void ModuleEditor::erase(qsizetype from, qsizetype to)
{
    removeText(current, from, to);
    cursor = from;
    anchor = from;
    column.reset();
    relayout();
}

void ModuleEditor::remember(Change change)
{
    redos.clear();
    const bool continues = change != Change::None && change == last;
    last = change;
    if (!continues)
        undos.push_back({current.text, current.spans, cursor});
}

void ModuleEditor::restore(std::vector<Snapshot> &from, std::vector<Snapshot> &to)
{
    if (from.empty())
        return;
    to.push_back({current.text, current.spans, cursor});
    const Snapshot back = std::move(from.back());
    from.pop_back();
    current.text = back.text;
    current.spans = back.spans;
    cursor = std::min(back.cursor, current.text.size());
    anchor = cursor;
    last = Change::None;
    relayout();
}

void ModuleEditor::relayout()
{
    shape = typeset(current, *library);
    lit = true;
    blink.start();
    place(toView);
    emit edited(current);
}

void ModuleEditor::commit()
{
    if (isHidden())
        return;
    blink.stop();
    hide();
    emit committed(current);
}

void ModuleEditor::cancel()
{
    if (isHidden())
        return;
    blink.stop();
    hide();
    emit cancelled(original);
}
