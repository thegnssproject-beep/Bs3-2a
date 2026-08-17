#pragma once

class CheckT
{
public:
    // Corrects time difference for beginning/end of week crossover
    // Input:
    //   time - Time difference in seconds
    // Output:
    //   Corrected time difference in seconds within [-302400, +302400]
    static double check(double time);
};