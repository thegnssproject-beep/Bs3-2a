#pragma once

#include <vector>
#include <string>
#include <cmath>
#include <iostream>
#include <algorithm>
#include <limits>
#include "Settings.h"
#include "Tracking.h"
#include "Ephemeris.h"

struct NavSolutions {
    std::vector<double> X;
    std::vector<double> Y;
    std::vector<double> Z;
    std::vector<double> dt;
    std::vector<double> latitude;
    std::vector<double> longitude;
    std::vector<double> height;
    std::vector<double> HDOP;
    std::vector<double> VDOP;
    std::vector<double> localTime;
    std::vector<long long> currMeasSample;

    // Polar sky view mapping
    std::vector<int> activePrns;
    std::vector<std::vector<double>> azimuth;
    std::vector<std::vector<double>> elevation;
};

class PostNavigation
{
public:
    static bool run(
        const std::vector<ChannelTrackResult>& trackResults,
        const Settings& settings,
        NavSolutions& navSolutions
    );
    static double check_t(double t);
    static void satpos(
        const std::vector<double>& transmitTime,
        const std::vector<int>& prnList,
        const std::vector<Ephemeris>& eph,
        std::vector<std::vector<double>>& satPositions,
        std::vector<double>& satClkCorr
    );

private:
    static void decodeBCNAV2(
        const std::vector<double>& I_P,
        Ephemeris& eph,
        double& subFrameStart,
        double& tow
    );
    static void calculatePseudoranges(
        const std::vector<ChannelTrackResult>& trackResults,
        const std::vector<double>& subFrameStart,
        const std::vector<double>& TOW,
        long long currMeasSample,
        double& localTime,
        const std::vector<int>& activeChnList,
        const Settings& settings,
        std::vector<double>& rawP,
        std::vector<double>& transmitTime
    );
    static void leastSquarePos(
        const std::vector<std::vector<double>>& satPos,
        const std::vector<double>& obsP,
        const Settings& settings,
        std::vector<double>& xyzdt,
        std::vector<double>& el,
        std::vector<double>& az,
        std::vector<double>& dop
    );
    static void cart2geo(
        double X,
        double Y,
        double Z,
        double& lat,
        double& lon,
        double& h
    );
};