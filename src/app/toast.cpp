#include "toast.h"

#include "theme.h"

#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>

namespace {

constexpr int kFadeMs = 220;
constexpr int kHoldMs = 2600;
constexpr int kBottom = 84;

} // namespace

Toast::Toast(QWidget *parent) : QLabel(parent)
{
    auto *opacity = new QGraphicsOpacityEffect(this);
    setGraphicsEffect(opacity);
    fade = new QPropertyAnimation(opacity, "opacity", this);
    fade->setDuration(kFadeMs);
    fade->setEasingCurve(QEasingCurve::OutCubic);
    connect(fade, &QPropertyAnimation::finished, this, [this] {
        if (fade->endValue().toDouble() == 0)
            hide();
    });
    hold.setSingleShot(true);
    connect(&hold, &QTimer::timeout, this, [this] {
        fade->setStartValue(1.0);
        fade->setEndValue(0.0);
        fade->start();
    });
    setAttribute(Qt::WA_TransparentForMouseEvents);
    hide();
}

void Toast::pop(const QString &message, Tone tone)
{
    const QColor edge = tone == Tone::Error ? theme::danger : theme::accent;
    setStyleSheet(QStringLiteral("QLabel { background: rgba(26,27,38,235); border: 1px solid %1;"
                                 " border-radius: 14px; padding: 9px 18px; font-weight: 600; }")
                      .arg(theme::faded(edge, 160).name(QColor::HexArgb)));
    setText(message);
    adjustSize();
    const QWidget *host = parentWidget();
    move((host->width() - width()) / 2, host->height() - height() - kBottom);
    raise();
    show();
    fade->stop();
    fade->setStartValue(0.0);
    fade->setEndValue(1.0);
    fade->start();
    hold.start(kHoldMs);
}
