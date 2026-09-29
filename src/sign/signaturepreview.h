#pragma once

#include "signature.h"

#include <QWidget>

// Paper card previewing the signature.
class SignaturePreview : public QWidget
{
public:
    explicit SignaturePreview(QWidget *parent = nullptr);
    void setSignature(const Signature &signature);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    Signature shown;
};
