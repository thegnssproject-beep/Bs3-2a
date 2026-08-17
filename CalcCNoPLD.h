#pragma once

#include <vector>
#include <cmath>
#include <numeric>
#include <algorithm>
#include "Settings.h"
#include "Tracking.h"

struct CNoPLDResult
{
    std::vector<double> CNo = std::vector<double>(3, 0.0);         // [0]=Data, [1]=Pilot, [2]=Total B2a
    std::vector<double> PllDetector = std::vector<double>(2, 0.0); // [0]=Data, [1]=Pilot
};

class CalcCNoPLD
{
public:
    static CNoPLDResult compute(const ChannelTrackResult& trackResults, const Settings& settings, int loopCnt);
};