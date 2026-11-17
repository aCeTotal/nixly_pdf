#include "markbar.h"

#include "app/theme.h"

#include <QColorDialog>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QMenu>
#include <QSignalBlocker>
#include <QToolButton>

namespace {

constexpr double kMinWidth = 0.5;
constexpr double kMaxWidth = 20;
constexpr double kMinSize = 6;
constexpr double kMaxSize = 96;

QDoubleSpinBox *spin(double low, double high, const QString &tip)
{
    auto *box = new QDoubleSpinBox;
    box->setRange(low, high);
    box->setDecimals(1);
    box->setSuffix(QStringLiteral(" pt"));
    box->setToolTip(tip);
    return box;
}

} // namespace

MarkBar::MarkBar(QWidget *parent)
    : QFrame(parent), ink(new QToolButton), fill(new QToolButton), width(spin(kMinWidth, kMaxWidth, tr("Line width"))),
      size(spin(kMinSize, kMaxSize, tr("Text size")))
{
    setObjectName("floatbar");
    theme::lift(this);
    hide();
    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(8, 6, 8, 6);
    row->setSpacing(6);
    ink->setToolTip(tr("Colour"));
    fill->setToolTip(tr("Fill"));
    fill->setPopupMode(QToolButton::InstantPopup);
    auto *menu = new QMenu(fill);
    menu->addAction(tr("No fill"), this, [this] {
        current.fill = QColor();
        theme::paintHollow(fill);
        emit changed(current);
    });
    menu->addAction(tr("Choose colour…"), this, [this] { pickFill(); });
    fill->setMenu(menu);
    auto *remove = new QToolButton;
    remove->setText(QStringLiteral("✕"));
    remove->setToolTip(tr("Delete"));
    for (QWidget *widget : std::initializer_list<QWidget *>{ink, fill, width, size, remove})
        row->addWidget(widget);

    connect(ink, &QToolButton::clicked, this, [this] { pickInk(); });
    connect(width, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        current.width = value;
        emit changed(current);
    });
    connect(size, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        current.size = value;
        emit changed(current);
    });
    connect(remove, &QToolButton::clicked, this, [this] { emit removeRequested(current.id); });
}

void MarkBar::present(const Mark &mark)
{
    current = mark;
    const QSignalBlocker blockWidth(width), blockSize(size);
    const bool drawn = mark.kind != MarkKind::Image;
    const bool filled = drawn && mark.kind != MarkKind::Arrow;
    ink->setVisible(drawn);
    fill->setVisible(filled);
    width->setVisible(drawn);
    size->setVisible(mark.kind == MarkKind::Callout);
    theme::paintInk(ink, mark.ink);
    if (mark.fill.isValid())
        theme::paintInk(fill, mark.fill);
    else
        theme::paintHollow(fill);
    width->setValue(mark.width);
    size->setValue(mark.size);
    adjustSize();
    show();
    raise();
}

void MarkBar::pickInk()
{
    const QColor picked = QColorDialog::getColor(current.ink, this, tr("Line colour"));
    if (!picked.isValid())
        return;
    current.ink = picked;
    theme::paintInk(ink, picked);
    emit changed(current);
}

void MarkBar::pickFill()
{
    const QColor start = current.fill.isValid() ? current.fill : Qt::white;
    const QColor picked = QColorDialog::getColor(start, this, tr("Fill colour"), QColorDialog::ShowAlphaChannel);
    if (!picked.isValid())
        return;
    current.fill = picked;
    theme::paintInk(fill, picked);
    emit changed(current);
}
