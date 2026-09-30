#include "editbar.h"

#include "toolicons.h"

#include <QButtonGroup>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPropertyAnimation>
#include <QToolButton>

namespace {

constexpr int kHeight = 52;
constexpr int kSlideMs = 200;
constexpr int kIcon = 22;
constexpr int kDividerHeight = 24;
constexpr int kText = 100;
constexpr int kDate = 101;

// Tool placing something on page.
struct Placing
{
    Tool tool;
    int id;
    const char *name;
    const char *hint;
};

// Tool acting on current page.
struct Acting
{
    Tool tool;
    void (EditBar::*signal)();
    const char *name;
    const char *hint;
};

const Placing kPlacing[] = {
    {Tool::Text, kText, QT_TRANSLATE_NOOP("EditBar", "Text"),
     QT_TRANSLATE_NOOP("EditBar", "Click where the text goes")},
    {Tool::Date, kDate, QT_TRANSLATE_NOOP("EditBar", "Date"),
     QT_TRANSLATE_NOOP("EditBar", "Click where today's date goes")},
    {Tool::Image, int(MarkKind::Image), QT_TRANSLATE_NOOP("EditBar", "Image"),
     QT_TRANSLATE_NOOP("EditBar", "Pick a picture, then click or drag its place")},
    {Tool::Arrow, int(MarkKind::Arrow), QT_TRANSLATE_NOOP("EditBar", "Arrow"),
     QT_TRANSLATE_NOOP("EditBar", "Drag from the tail to the point")},
    {Tool::Callout, int(MarkKind::Callout), QT_TRANSLATE_NOOP("EditBar", "Callout"),
     QT_TRANSLATE_NOOP("EditBar", "Press where it points, drag to the text bubble")},
    {Tool::Rectangle, int(MarkKind::Rectangle), QT_TRANSLATE_NOOP("EditBar", "Rectangle"),
     QT_TRANSLATE_NOOP("EditBar", "Drag a box")},
    {Tool::Ellipse, int(MarkKind::Ellipse), QT_TRANSLATE_NOOP("EditBar", "Circle"),
     QT_TRANSLATE_NOOP("EditBar", "Drag a circle or ellipse")},
    {Tool::Cloud, int(MarkKind::Cloud), QT_TRANSLATE_NOOP("EditBar", "Cloud"),
     QT_TRANSLATE_NOOP("EditBar", "Drag a cloud around something")},
};

const Acting kActing[] = {
    {Tool::Recognize, &EditBar::recognize, QT_TRANSLATE_NOOP("EditBar", "Recognise text"),
     QT_TRANSLATE_NOOP("EditBar", "Turn a scanned page into editable text")},
    {Tool::Blank, &EditBar::addBlank, QT_TRANSLATE_NOOP("EditBar", "Blank page"),
     QT_TRANSLATE_NOOP("EditBar", "Add an empty page after this one")},
    {Tool::Insert, &EditBar::insertFile, QT_TRANSLATE_NOOP("EditBar", "Insert PDF"),
     QT_TRANSLATE_NOOP("EditBar", "Insert pages from another PDF after this one")},
    {Tool::Delete, &EditBar::deletePage, QT_TRANSLATE_NOOP("EditBar", "Delete page"),
     QT_TRANSLATE_NOOP("EditBar", "Remove the current page")},
};

QToolButton *toolButton(Tool tool, const char *name, const char *hint)
{
    auto *button = new QToolButton;
    button->setObjectName("tool");
    button->setIcon(toolIcon(tool));
    button->setIconSize(QSize(kIcon, kIcon));
    button->setCursor(Qt::PointingHandCursor);
    const QString title = QCoreApplication::translate("EditBar", name);
    button->setToolTip(QStringLiteral("<b>%1</b><br>%2").arg(title, QCoreApplication::translate("EditBar", hint)));
    return button;
}

QFrame *divider()
{
    auto *line = new QFrame;
    line->setObjectName("divider");
    line->setFixedSize(1, kDividerHeight);
    return line;
}

} // namespace

EditBar::EditBar(QWidget *parent)
    : QFrame(parent), slide(new QPropertyAnimation(this, "maximumHeight", this)), placing(new QButtonGroup(this))
{
    setObjectName("editbar");
    setMaximumHeight(0);
    slide->setDuration(kSlideMs);
    slide->setEasingCurve(QEasingCurve::OutCubic);

    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(16, 7, 16, 7);
    row->setSpacing(4);
    for (const Placing &entry : kPlacing) {
        QToolButton *tool = toolButton(entry.tool, entry.name, entry.hint);
        tool->setCheckable(true);
        placing->addButton(tool, entry.id);
        row->addWidget(tool);
        if (entry.tool == Tool::Image)
            row->addWidget(divider());
    }
    row->addWidget(divider());
    for (const Acting &entry : kActing) {
        QToolButton *tool = toolButton(entry.tool, entry.name, entry.hint);
        connect(tool, &QToolButton::clicked, this, entry.signal);
        row->addWidget(tool);
    }
    row->addSpacing(12);
    auto *hint = new QLabel(tr("Click text to lift it  ·  Drag to move  ·  Double-click to type"));
    hint->setObjectName("hint");
    row->addWidget(hint, 1, Qt::AlignRight);
    connect(placing, &QButtonGroup::idClicked, this, [this](int id) {
        if (id == kText)
            emit addText();
        else if (id == kDate)
            emit addDate();
        else
            emit drawMark(MarkKind(id));
    });
}

void EditBar::reveal()
{
    slideTo(kHeight);
}

void EditBar::conceal()
{
    release();
    slideTo(0);
}

void EditBar::release()
{
    placing->setExclusive(false);
    if (QAbstractButton *armed = placing->checkedButton())
        armed->setChecked(false);
    placing->setExclusive(true);
}

void EditBar::slideTo(int height)
{
    slide->stop();
    slide->setStartValue(maximumHeight());
    slide->setEndValue(height);
    slide->start();
}
