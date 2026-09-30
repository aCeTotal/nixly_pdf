#include "modulebar.h"

#include "app/theme.h"
#include "sign/scriptfonts.h"

#include <QColorDialog>
#include <QComboBox>
#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLocale>
#include <QSignalBlocker>
#include <QToolButton>

namespace {

const char *kBundled[] = {"Liberation Sans", "Liberation Serif", "Liberation Mono", "Carlito",
                          "Caladea",         "DejaVu Sans",      "DejaVu Serif"};
constexpr double kMinSize = 4;
constexpr double kMaxSize = 400;
constexpr int kOriginalRow = -1;

} // namespace

ModuleBar::ModuleBar(QWidget *parent)
    : QFrame(parent), family(new QComboBox), size(new QDoubleSpinBox), bold(toggle("B", tr("Bold"))),
      italic(toggle("I", tr("Italic"))), ink(new QToolButton), date(new QDateEdit)
{
    setObjectName("floatbar");
    theme::lift(this);
    hide();
    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(8, 6, 8, 6);
    row->setSpacing(6);
    family->setMinimumWidth(170);
    family->setMaxVisibleItems(20);
    size->setRange(kMinSize, kMaxSize);
    size->setDecimals(1);
    size->setSuffix(QStringLiteral(" pt"));
    date->setCalendarPopup(true);
    ink->setToolTip(tr("Colour"));
    auto *remove = new QToolButton;
    remove->setText(QStringLiteral("✕"));
    remove->setToolTip(tr("Delete"));
    for (QWidget *widget : std::initializer_list<QWidget *>{family, size, bold, italic, ink, date, remove})
        row->addWidget(widget);

    connect(family, &QComboBox::activated, this, [this](int row) { chooseFamily(row); });
    connect(size, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        current.size = value;
        emit changed(current);
    });
    connect(bold, &QToolButton::toggled, this, [this] { setStyle(); });
    connect(italic, &QToolButton::toggled, this, [this] { setStyle(); });
    connect(ink, &QToolButton::clicked, this, [this] { pickColour(); });
    connect(date, &QDateEdit::dateChanged, this, [this](QDate day) {
        current.date = day;
        current.text = QLocale().toString(day, QLocale::ShortFormat);
        emit changed(current);
    });
    connect(remove, &QToolButton::clicked, this, [this] { emit removeRequested(current.id); });
}

QToolButton *ModuleBar::toggle(const QString &text, const QString &tip)
{
    auto *button = new QToolButton;
    button->setText(text);
    button->setToolTip(tip);
    button->setCheckable(true);
    return button;
}

void ModuleBar::present(const Module &module, const QString &originalFamily)
{
    current = module;
    original = originalFamily;
    const QSignalBlocker blockFamily(family), blockSize(size), blockBold(bold), blockItalic(italic), blockDate(date);
    family->clear();
    if (module.originalFont)
        family->addItem(tr("Original — %1").arg(originalFamily), kOriginalRow);
    for (const char *name : kBundled)
        family->addItem(QString::fromLatin1(name));
    for (const ScriptFont &script : scriptFonts())
        family->addItem(script.family);
    family->insertSeparator(family->count());
    family->addItems(QFontDatabase::families());
    const int active = module.font.pageFont ? 0 : family->findText(module.font.family);
    family->setCurrentIndex(std::max(0, active));
    size->setValue(module.size);
    bold->setChecked(module.font.bold);
    italic->setChecked(module.font.italic);
    theme::paintInk(ink, module.color);
    date->setVisible(module.kind == ModuleKind::Date);
    date->setDate(module.date.isValid() ? module.date : QDate::currentDate());
    adjustSize();
    show();
    raise();
}

void ModuleBar::chooseFamily(int row)
{
    const bool originalRow = family->itemData(row).toInt() == kOriginalRow && current.originalFont;
    current.font.pageFont = originalRow ? current.originalFont : 0;
    current.font.family = originalRow ? QString() : family->itemText(row);
    emit changed(current);
}

void ModuleBar::setStyle()
{
    current.font.bold = bold->isChecked();
    current.font.italic = italic->isChecked();
    if (current.font.pageFont) {
        current.font.pageFont = 0;
        current.font.family = original;
    }
    emit changed(current);
}

void ModuleBar::pickColour()
{
    const QColor picked = QColorDialog::getColor(current.color, this, tr("Text colour"));
    if (!picked.isValid())
        return;
    current.color = picked;
    theme::paintInk(ink, picked);
    emit changed(current);
}
