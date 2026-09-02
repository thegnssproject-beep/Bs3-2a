#pragma once

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include <vector>
#include <fstream>
#include <complex>
#include <string>
#include "Settings.h"
#include "PreRun.h"

struct ChannelTrackResult {
    int PRN = 0;
    int status = 0;
    std::vector<double> absoluteSample;
    std::vector<double> codeFreq;
    std::vector<double> carrFreq;
    std::vector<double> remCodePhase;

    std::vector<double> I_E, I_P, I_L;
    std::vector<double> Q_E, Q_P, Q_L;

    std::vector<double> Pilot_I_E, Pilot_I_P, Pilot_I_L;
    std::vector<double> Pilot_Q_E, Pilot_Q_P, Pilot_Q_L;

    std::vector<double> dllError;
    std::vector<double> dllDiscrFilt;
    std::vector<double> pllError;
    std::vector<double> pllDiscrFilt;
    std::vector<double> CNo;
};

class Tracking
{
public:
    static void calcLoopCoef(double noiseBW, double dampingRatio, double k, double& tau1, double& tau2);
    static void calcLoopCoefCarr(const Settings& s, double& pf3, double& pf2, double& pf1);

    static std::vector<ChannelTrackResult> run(
        std::ifstream& fid,
        std::vector<Channel>& channels,
        const Settings& settings
    );
};