#include "lockdialog.h"

#include "theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

constexpr int kMinLength = 8;
constexpr int kFieldWidth = 340;

QLineEdit *secret()
{
    auto *field = new QLineEdit;
    field->setEchoMode(QLineEdit::Password);
    field->setMinimumWidth(kFieldWidth);
    return field;
}

} // namespace

LockDialog::LockDialog(QWidget *parent)
    : QDialog(parent), first(secret()), second(secret()), hint(new QLabel),
      lock(new QPushButton(tr("Encrypt and save")))
{
    setWindowTitle(tr("Save encrypted"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 20);
    layout->setSpacing(10);
    layout->addWidget(theme::caption(tr("Passphrase")));
    layout->addWidget(first);
    layout->addWidget(theme::caption(tr("Repeat passphrase")));
    layout->addWidget(second);
    hint->setObjectName("hint");
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto *cancel = new QPushButton(tr("Cancel"));
    lock->setObjectName("primary");
    lock->setDefault(true);
    auto *row = new QHBoxLayout;
    row->addStretch();
    row->addWidget(cancel);
    row->addWidget(lock);
    layout->addSpacing(6);
    layout->addLayout(row);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(lock, &QPushButton::clicked, this, &QDialog::accept);
    connect(first, &QLineEdit::textChanged, this, [this] { check(); });
    connect(second, &QLineEdit::textChanged, this, [this] { check(); });
    check();
}

QString LockDialog::passphrase() const
{
    return first->text();
}

void LockDialog::check()
{
    const bool longEnough = first->text().size() >= kMinLength;
    const bool same = first->text() == second->text();
    lock->setEnabled(longEnough && same);
    if (!longEnough)
        hint->setText(tr("At least %1 characters. Longer is stronger.").arg(kMinLength));
    else if (!same)
        hint->setText(tr("The passphrases differ."));
    else
        hint->setText(tr("AES-256 encryption. Opens in Acrobat and any modern PDF reader with this passphrase."));
}
