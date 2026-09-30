#include "moduleeditor.h"

#include "app/theme.h"

#include <QKeyEvent>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>
#include <algorithm>

namespace {

constexpr int kPadding = 4;
constexpr int kBorder = 2;
constexpr double kSlack = 1.1;
constexpr double kRoom = 1.5;
constexpr double kLeading = 1.2;
constexpr int kSpanTag = QTextFormat::UserProperty;
constexpr double kFullStretch = 100;

double leadingOf(const Module &module)
{
    return module.leading > 0 ? module.leading : module.size * kLeading;
}

// Family a style renders in.
QString familyIn(const Layout &layout, int style, const QString &fallback)
{
    const auto hit = std::ranges::find(layout.runs, style, &GlyphRun::style);
    return hit == layout.runs.end() ? fallback : hit->face->family;
}

QFont faceFor(const QString &family, const FontChoice &choice, QSizeF em)
{
    QFont face(family);
    face.setBold(choice.bold);
    face.setItalic(choice.italic);
    face.setPixelSize(std::max(1, qRound(em.height())));
    face.setStretch(qRound(em.width() / em.height() * kFullStretch));
    return face;
}

void addPiece(std::vector<Span> &spans, const QTextFragment &piece, const std::vector<Span> &styles)
{
    const int tag = piece.charFormat().intProperty(kSpanTag);
    if (!tag)
        return;
    Span span = styles[size_t(tag - 1)];
    span.start = piece.position();
    span.length = piece.length();
    spans.push_back(span);
}

// Spans read from edited text.
std::vector<Span> spansIn(const QTextDocument &doc, const std::vector<Span> &styles)
{
    std::vector<Span> spans;
    for (QTextBlock block = doc.begin(); block.isValid(); block = block.next()) {
        for (auto it = block.begin(); !it.atEnd(); ++it)
            addPiece(spans, it.fragment(), styles);
    }
    return spans;
}

} // namespace

ModuleEditor::ModuleEditor(QWidget *parent) : QPlainTextEdit(parent)
{
    hide();
    setFrameShape(QFrame::NoFrame);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    document()->setDocumentMargin(kPadding);
    connect(this, &QPlainTextEdit::textChanged, this, [this] { fit(); });
}

void ModuleEditor::open(const Module &module, FontLibrary &fonts, double zoom)
{
    current = module;
    library = &fonts;
    scale = zoom;
    const Layout shape = typeset(module, fonts);
    setStyleSheet(QStringLiteral("QPlainTextEdit { background: transparent; color: %1; border: 1.5px dashed %2;"
                                 " border-radius: 4px; selection-background-color: %3; }")
                      .arg(module.color.name(), theme::accent.name(),
                           theme::faded(theme::accent, 90).name(QColor::HexArgb)));
    const double pixels = module.size * zoom;
    setFont(faceFor(familyIn(shape, 0, module.font.family), module.font, QSizeF(pixels * module.stretch, pixels)));
    setLineWrapMode(module.width > 0 ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
    document()->setUndoRedoEnabled(false);
    setPlainText(module.text);
    dress(shape);
    document()->setUndoRedoEnabled(true);
    fit();
    show();
    raise();
    setFocus();
    selectAll();
}

// Line height and span formats.
void ModuleEditor::dress(const Layout &shape)
{
    QTextCursor cursor(document());
    cursor.beginEditBlock();
    cursor.select(QTextCursor::Document);
    QTextBlockFormat lines;
    lines.setLineHeight(leadingOf(current) * scale, QTextBlockFormat::FixedHeight);
    cursor.mergeBlockFormat(lines);
    for (size_t i = 0; i < current.spans.size(); ++i) {
        const Span &span = current.spans[i];
        const QString family = familyIn(shape, int(i + 1), span.font.family);
        QTextCharFormat format;
        const double pixels = current.size * span.scale * scale;
        format.setFont(faceFor(family, span.font, QSizeF(pixels * current.stretch, pixels)));
        format.setProperty(kSpanTag, int(i + 1));
        if (span.color.isValid())
            format.setForeground(span.color);
        cursor.setPosition(span.start);
        cursor.setPosition(span.start + span.length, QTextCursor::KeepAnchor);
        cursor.setCharFormat(format);
    }
    cursor.endEditBlock();
}

// First baseline meets anchor.
void ModuleEditor::place(QPointF anchor)
{
    const QTextBlock first = document()->firstBlock();
    const QTextLine line = first.layout() ? first.layout()->lineAt(0) : QTextLine();
    const double ascent = line.isValid() ? line.y() + line.ascent() : QFontMetricsF(font()).ascent();
    const QPointF top = blockBoundingGeometry(first).translated(contentOffset()).topLeft();
    const QPointF baseline = QPointF(viewport()->mapTo(this, QPoint(0, 0))) + top + QPointF(0, ascent);
    move((anchor - baseline).toPoint());
}

void ModuleEditor::fit()
{
    Module typed = current;
    typed.text = toPlainText();
    typed.spans = spansIn(*document(), current.spans);
    const QRectF bounds = typeset(typed, *library).bounds;
    const int chrome = (kPadding + kBorder) * 2;
    const double room = current.width > 0 ? 0 : current.size * kRoom;
    const int wide = int((bounds.width() * current.stretch * kSlack + room) * scale) + chrome;
    resize(wide, int(bounds.height() * scale) + chrome + kPadding);
}

void ModuleEditor::keyPressEvent(QKeyEvent *event)
{
    const bool enter = event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter;
    if (event->key() == Qt::Key_Escape) {
        cancel();
        return;
    }
    if (enter && !(event->modifiers() & Qt::ShiftModifier)) {
        commit();
        return;
    }
    QPlainTextEdit::keyPressEvent(event);
}

void ModuleEditor::focusOutEvent(QFocusEvent *event)
{
    QPlainTextEdit::focusOutEvent(event);
    commit();
}

void ModuleEditor::commit()
{
    if (isHidden())
        return;
    hide();
    current.text = toPlainText();
    current.spans = spansIn(*document(), current.spans);
    emit committed(current);
}

void ModuleEditor::cancel()
{
    if (isHidden())
        return;
    hide();
    emit cancelled();
}
