#pragma once

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

#include <vector>
#include <cmath>

#include "Settings.h"
#include "Tracking.h"
#include "Ephemeris.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct NavSolutions {
    std::vector<int> activePrns;
    std::vector<std::vector<double>> azimuth;
    std::vector<std::vector<double>> elevation;
    std::vector<double> latitude;
    std::vector<double> longitude;
    std::vector<double> height;
    std::vector<double> X;
    std::vector<double> Y;
    std::vector<double> Z;
    std::vector<double> dt;
    std::vector<double> HDOP;
    std::vector<double> VDOP;
    std::vector<double> localTime;
    std::vector<int> currMeasSample;
};

class PostNavigation {
public:
    static bool run(
        const std::vector<ChannelTrackResult>& trackResults,
        const Settings& settings,
        NavSolutions& navSolutions
    );

    static void cart2geo(
        double X, double Y, double Z,
        double& lat, double& lon, double& h
    ) noexcept;
};