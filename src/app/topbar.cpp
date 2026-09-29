#include "topbar.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>

TopBar::TopBar(QMenu *fileMenu, QWidget *parent)
    : QFrame(parent), title(new QLabel), modes(new QButtonGroup(this))
{
    setObjectName("bar");
    setFixedHeight(52);
    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(12, 0, 12, 0);

    auto *file = new QToolButton;
    file->setObjectName("file");
    file->setText(tr("File"));
    file->setMenu(fileMenu);
    file->setPopupMode(QToolButton::InstantPopup);
    file->setCursor(Qt::PointingHandCursor);
    row->addWidget(file);
    row->addStretch();
    title->setObjectName("title");
    row->addWidget(title);
    row->addStretch();

    auto *segment = new QFrame;
    segment->setObjectName("segment");
    auto *switches = new QHBoxLayout(segment);
    switches->setContentsMargins(3, 3, 3, 3);
    switches->setSpacing(2);
    const std::pair<Mode, QString> entries[] = {{Mode::Read, tr("Read")}, {Mode::Edit, tr("Edit")}, {Mode::Sign, tr("Sign")}};
    for (const auto &[mode, label] : entries) {
        auto *button = new QPushButton(label);
        button->setObjectName("mode");
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
        modes->addButton(button, int(mode));
        switches->addWidget(button);
    }
    modes->button(int(Mode::Read))->setChecked(true);
    row->addWidget(segment);
    connect(modes, &QButtonGroup::idClicked, this, [this](int id) { emit modeChosen(Mode(id)); });
    setModesEnabled(false);
}

void TopBar::setTitle(const QString &text)
{
    title->setText(text);
}

void TopBar::setMode(Mode mode)
{
    modes->button(int(mode))->setChecked(true);
}

void TopBar::setModesEnabled(bool enabled)
{
    for (QAbstractButton *button : modes->buttons())
        button->setEnabled(enabled);
}
