#pragma once

#include <QLabel>
#include <QTimer>

class QPropertyAnimation;

enum class Tone { Info, Error };

// Fading message over parent.
class Toast : public QLabel
{
public:
    explicit Toast(QWidget *parent);
    void pop(const QString &message, Tone tone);

private:
    QPropertyAnimation *fade;
    QTimer hold;
};
