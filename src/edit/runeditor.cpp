#include "runeditor.h"

#include "app/theme.h"
#include "pdf/fontname.h"
#include "pdf/textrun.h"

#include <QGraphicsDropShadowEffect>
#include <QKeyEvent>

namespace {

constexpr int kPadding = 6;

} // namespace

RunEditor::RunEditor(QWidget *parent) : QLineEdit(parent)
{
    hide();
    auto *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(24);
    shadow->setOffset(0, 4);
    shadow->setColor(theme::faded(theme::accent, 120));
    setGraphicsEffect(shadow);
    connect(this, &QLineEdit::returnPressed, this, [this] { commit(); });
    connect(this, &QLineEdit::textChanged, this, [this] { fit(); });
}

void RunEditor::open(const TextRun &run)
{
    const QColor color = QColor::fromRgb(run.color);
    setStyleSheet(QStringLiteral("QLineEdit { background: #ffffff; color: %1; border: 2px solid %2;"
                                 " border-radius: 6px; padding: 0 %3px; selection-background-color: %4; }")
                      .arg(color.name(), theme::accent.name(), QString::number(kPadding - 2),
                           theme::faded(theme::accent, 90).name(QColor::HexArgb)));
    QFont face(familyOf(run.font));
    face.setStyleHint(run.serif ? QFont::Serif : QFont::SansSerif);
    face.setBold(run.bold);
    face.setItalic(run.italic);
    setFont(face);
    setText(run.text);
    show();
    raise();
    setFocus();
    selectAll();
}

void RunEditor::place(const QRectF &box, double pixelSize)
{
    QFont face = font();
    face.setPixelSize(qMax(8, qRound(pixelSize)));
    setFont(face);
    const QRect area = box.adjusted(-kPadding, -kPadding / 2.0, kPadding * 4, kPadding / 2.0).toAlignedRect();
    baseWidth = area.width();
    setGeometry(area.adjusted(0, 0, 0, qMax(0, fontMetrics().height() + kPadding - area.height())));
    fit();
}

void RunEditor::fit()
{
    resize(qMax(baseWidth, fontMetrics().horizontalAdvance(text()) + kPadding * 5), height());
}

void RunEditor::keyPressEvent(QKeyEvent *event)
{
    if (event->key() != Qt::Key_Escape) {
        QLineEdit::keyPressEvent(event);
        return;
    }
    cancel();
}

void RunEditor::focusOutEvent(QFocusEvent *event)
{
    QLineEdit::focusOutEvent(event);
    commit();
}

void RunEditor::commit()
{
    if (isHidden())
        return;
    hide();
    emit committed(text());
}

void RunEditor::cancel()
{
    if (isHidden())
        return;
    hide();
    emit cancelled();
}
