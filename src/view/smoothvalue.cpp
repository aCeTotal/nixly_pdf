#include "smoothvalue.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr double kRate = 16.0;

} // namespace

void SmoothValue::advance(double seconds)
{
    current = goal + (current - goal) * std::exp(-kRate * seconds);
    if (std::fabs(current - goal) < precision)
        current = goal;
}

void SmoothValue::clamp(double lo, double hi)
{
    current = std::clamp(current, lo, hi);
    goal = std::clamp(goal, lo, hi);
}
