#pragma once

// Exponentially eased value.
class SmoothValue
{
public:
    explicit SmoothValue(double precision = 0.01) : precision(precision) {}

    void jump(double v) { current = goal = v; }
    void setTarget(double v) { goal = v; }
    void shift(double d) { current += d; goal += d; }
    void clamp(double lo, double hi);
    double value() const { return current; }
    double target() const { return goal; }
    bool moving() const { return current != goal; }

    // Advances by elapsed seconds.
    void advance(double seconds);

private:
    double current = 0;
    double goal = 0;
    double precision;
};
