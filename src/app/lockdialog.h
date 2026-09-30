#pragma once

#include <QDialog>

class QLabel;
class QLineEdit;
class QPushButton;

// New passphrase, typed twice.
class LockDialog : public QDialog
{
public:
    explicit LockDialog(QWidget *parent);
    QString passphrase() const;

private:
    void check();

    QLineEdit *first;
    QLineEdit *second;
    QLabel *hint;
    QPushButton *lock;
};
