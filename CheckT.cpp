#include "CheckT.h"

double CheckT::check(double time)
{
    const double half_week = 302400.0; // Half week in seconds
    double corrTime = time;

    // Account for beginning or end of week crossover
    if (time > half_week) {
        corrTime = time - 2.0 * half_week;
    }
    else if (time < -half_week) {
        corrTime = time + 2.0 * half_week;
    }

    return corrTime;
}