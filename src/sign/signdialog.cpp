#include "signdialog.h"

#include "scriptfonts.h"
#include "signaturepreview.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QColorDialog>
#include <QDateEdit>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

namespace {

const QColor kInks[] = {QColor(24, 38, 92), QColor(20, 20, 24), QColor(30, 80, 200), QColor(22, 101, 52),
                        QColor(128, 24, 40)};
const char *kRainbow = "QPushButton { background: qconicalgradient(cx:0.5, cy:0.5, angle:0, stop:0 #f7768e,"
                       " stop:0.33 #e0af68, stop:0.66 #7aa2f7, stop:1 #f7768e); }";
constexpr int kStyleColumns = 2;
constexpr int kStylePixels = 26;
constexpr int kSwatch = 30;
constexpr int kMinSize = 18;
constexpr int kMaxSize = 54;

QLabel *caption(const QString &text)
{
    auto *label = new QLabel(text.toUpper());
    label->setObjectName("caption");
    return label;
}

void paintSwatch(QPushButton *swatch, const QColor &color)
{
    swatch->setStyleSheet(QStringLiteral("QPushButton { background: %1; }").arg(color.name()));
}

} // namespace

SignDialog::SignDialog(const Signature &initial, QWidget *parent)
    : QDialog(parent), chosen(initial), preview(new SignaturePreview), name(new QLineEdit(initial.name)),
      styles(new QButtonGroup(this)), inks(new QButtonGroup(this)), place(new QPushButton(tr("Place signature")))
{
    setWindowTitle(tr("Signature"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 20);
    layout->setSpacing(10);
    layout->addWidget(preview);

    layout->addWidget(caption(tr("Name")));
    name->setPlaceholderText(tr("Your name"));
    layout->addWidget(name);
    connect(name, &QLineEdit::textChanged, this, [this](const QString &text) {
        chosen.name = text;
        refresh();
    });

    layout->addWidget(caption(tr("Handwriting")));
    layout->addLayout(buildStyles());
    layout->addWidget(caption(tr("Ink and size")));
    layout->addLayout(buildInks());

    layout->addLayout(buildDate());
    layout->addWidget(caption(tr("Extra text")));
    layout->addWidget(buildNote());
    layout->addSpacing(6);
    layout->addLayout(buildButtons());
    refresh();
}

QHBoxLayout *SignDialog::buildDate()
{
    auto *dated = new QCheckBox(tr("Add date"));
    dated->setChecked(chosen.dated);
    auto *date = new QDateEdit(chosen.date);
    date->setCalendarPopup(true);
    date->setEnabled(chosen.dated);
    auto *row = new QHBoxLayout;
    row->addWidget(dated);
    row->addWidget(date, 1);
    connect(dated, &QCheckBox::toggled, this, [this, date](bool on) {
        chosen.dated = on;
        date->setEnabled(on);
        refresh();
    });
    connect(date, &QDateEdit::dateChanged, this, [this](QDate day) {
        chosen.date = day;
        refresh();
    });
    return row;
}

QPlainTextEdit *SignDialog::buildNote()
{
    auto *note = new QPlainTextEdit(chosen.note);
    note->setPlaceholderText(tr("Title, company, place \u2026"));
    note->setFixedHeight(64);
    connect(note, &QPlainTextEdit::textChanged, this, [this, note] {
        chosen.note = note->toPlainText();
        refresh();
    });
    return note;
}

QHBoxLayout *SignDialog::buildButtons()
{
    auto *cancel = new QPushButton(tr("Cancel"));
    place->setObjectName("primary");
    place->setDefault(true);
    auto *row = new QHBoxLayout;
    row->addStretch();
    row->addWidget(cancel);
    row->addWidget(place);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(place, &QPushButton::clicked, this, &QDialog::accept);
    return row;
}

QGridLayout *SignDialog::buildStyles()
{
    auto *grid = new QGridLayout;
    grid->setSpacing(8);
    const std::vector<ScriptFont> &fonts = scriptFonts();
    for (int i = 0; i < int(fonts.size()); ++i) {
        auto *style = new QPushButton;
        style->setObjectName("style");
        style->setCheckable(true);
        style->setChecked(i == chosen.font);
        QFont face(fonts[size_t(i)].family);
        face.setPixelSize(kStylePixels);
        style->setFont(face);
        style->setToolTip(fonts[size_t(i)].family);
        styles->addButton(style, i);
        grid->addWidget(style, i / kStyleColumns, i % kStyleColumns);
    }
    connect(styles, &QButtonGroup::idClicked, this, [this](int id) {
        chosen.font = id;
        refresh();
    });
    return grid;
}

QHBoxLayout *SignDialog::buildInks()
{
    auto *row = new QHBoxLayout;
    row->setSpacing(10);
    for (const QColor &ink : kInks) {
        QPushButton *swatch = addSwatch(row);
        swatch->setChecked(ink == chosen.color);
        paintSwatch(swatch, ink);
        connect(swatch, &QPushButton::clicked, this, [this, ink] {
            chosen.color = ink;
            refresh();
        });
    }
    const bool preset = inks->checkedButton();
    QPushButton *custom = addSwatch(row);
    custom->setToolTip(tr("Custom colour"));
    custom->setChecked(!preset);
    if (preset)
        custom->setStyleSheet(QString::fromLatin1(kRainbow));
    else
        paintSwatch(custom, chosen.color);
    connect(custom, &QPushButton::clicked, this, [this, custom] {
        const QColor picked = QColorDialog::getColor(chosen.color, this, tr("Ink colour"));
        if (!picked.isValid())
            return;
        chosen.color = picked;
        paintSwatch(custom, picked);
        refresh();
    });
    row->addSpacing(12);
    row->addWidget(buildSize(), 1);
    return row;
}

QPushButton *SignDialog::addSwatch(QHBoxLayout *row)
{
    auto *swatch = new QPushButton;
    swatch->setObjectName("swatch");
    swatch->setCheckable(true);
    swatch->setFixedSize(kSwatch, kSwatch);
    inks->addButton(swatch);
    row->addWidget(swatch);
    return swatch;
}

QSlider *SignDialog::buildSize()
{
    auto *size = new QSlider(Qt::Horizontal);
    size->setRange(kMinSize, kMaxSize);
    size->setValue(int(chosen.size));
    connect(size, &QSlider::valueChanged, this, [this](int value) {
        chosen.size = value;
        refresh();
    });
    return size;
}

void SignDialog::refresh()
{
    preview->setSignature(chosen);
    place->setEnabled(!chosen.name.trimmed().isEmpty());
    const std::vector<ScriptFont> &fonts = scriptFonts();
    for (QAbstractButton *style : styles->buttons()) {
        const int id = styles->id(style);
        style->setText(chosen.name.isEmpty() ? fonts[size_t(id)].family : chosen.name);
    }
}
