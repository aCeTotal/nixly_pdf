#include "ticker.h"

#include <QGuiApplication>
#include <QScreen>

namespace {

constexpr double kMaxStep = 0.05;

int frameInterval()
{
    const QScreen *screen = QGuiApplication::primaryScreen();
    const double rate = screen ? screen->refreshRate() : 60.0;
    return qMax(4, int(1000.0 / rate));
}

} // namespace

Ticker::Ticker(QObject *owner, std::function<bool(double)> frame) : frame(std::move(frame))
{
    timer.setTimerType(Qt::PreciseTimer);
    timer.setInterval(frameInterval());
    QObject::connect(&timer, &QTimer::timeout, owner, [this] {
        const double seconds = qMin(kMaxStep, clock.restart() / 1000.0);
        if (!this->frame(seconds))
            timer.stop();
    });
}

void Ticker::start()
{
    if (timer.isActive())
        return;
    clock.start();
    timer.start();
}
