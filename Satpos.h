#pragma once

#include <vector>
#include <string>
#include <cmath>
#include "Ephemeris.h"
#include "CheckT.h"

class Satpos
{
public:
    static void computePositions(
        const std::vector<double>& transmitTime,
        const std::vector<int>& prnList,
        const std::vector<Ephemeris>& eph,
        std::vector<std::vector<double>>& satPositions,
        std::vector<double>& satClkCorr
    );

    static void computePositionsAndVelocities(
        const std::vector<double>& transmitTime,
        const std::vector<int>& prnList,
        const std::vector<Ephemeris>& eph,
        std::vector<std::vector<double>>& satPositions,
        std::vector<double>& satClkCorr,
        std::vector<std::vector<double>>& satVelocities,
        std::vector<double>& satClkDrift
    );
};