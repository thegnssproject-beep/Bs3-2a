#pragma once

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

    // Data Channel Prompt/Early/Late Correlators
    std::vector<double> I_E, I_P, I_L;
    std::vector<double> Q_E, Q_P, Q_L;

    // Pilot Channel Correlators (for BDS-3 B2a Joint Tracking & C/N0)
    std::vector<double> Pilot_I_E, Pilot_I_P, Pilot_I_L;
    std::vector<double> Pilot_Q_E, Pilot_Q_P, Pilot_Q_L;

    std::vector<double> dllError;
    std::vector<double> pllError;
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