#include "editbar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPropertyAnimation>
#include <QPushButton>

namespace {

constexpr int kHeight = 50;
constexpr int kSlideMs = 200;

} // namespace

EditBar::EditBar(QWidget *parent) : QFrame(parent), slide(new QPropertyAnimation(this, "maximumHeight", this))
{
    setObjectName("editbar");
    setMaximumHeight(0);
    slide->setDuration(kSlideMs);
    slide->setEasingCurve(QEasingCurve::OutCubic);

    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(16, 8, 16, 8);
    row->setSpacing(8);
    const std::pair<QString, void (EditBar::*)()> tools[] = {
        {tr("+  Blank page"), &EditBar::addBlank},
        {tr("Insert PDF…"), &EditBar::insertFile},
        {tr("Delete page"), &EditBar::deletePage},
    };
    for (const auto &[label, signal] : tools) {
        auto *button = new QPushButton(label);
        button->setCursor(Qt::PointingHandCursor);
        row->addWidget(button);
        connect(button, &QPushButton::clicked, this, signal);
    }
    row->addSpacing(12);
    auto *hint = new QLabel(tr("Click any text to rewrite it  ·  Drag pages in the sidebar to reorder"));
    hint->setObjectName("hint");
    row->addWidget(hint, 1);
}

void EditBar::reveal()
{
    slideTo(kHeight);
}

void EditBar::conceal()
{
    slideTo(0);
}

void EditBar::slideTo(int height)
{
    slide->stop();
    slide->setStartValue(maximumHeight());
    slide->setEndValue(height);
    slide->start();
}
