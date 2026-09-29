#pragma once

#include <QElapsedTimer>
#include <QTimer>
#include <functional>

// Frame clock while animations run.
class Ticker
{
public:
    Ticker(QObject *owner, std::function<bool(double)> frame);
    void start();

private:
    QTimer timer;
    QElapsedTimer clock;
    std::function<bool(double)> frame;
};
