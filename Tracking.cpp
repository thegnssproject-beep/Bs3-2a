#include "Tracking.h"
#include "Acquisition.h"
#include "CalcCNoPLD.h"
#include <cmath>
#include <iostream>
#include <algorithm>

void Tracking::calcLoopCoef(double noiseBW, double dampingRatio, double k, double& tau1, double& tau2)
{
    double wn = noiseBW * 8.0 * dampingRatio / (4.0 * dampingRatio * dampingRatio + 1.0);
    tau1 = k / (wn * wn);
    tau2 = 2.0 * dampingRatio / wn;
}

void Tracking::calcLoopCoefCarr(const Settings& s, double& pf3, double& pf2, double& pf1)
{
    double Bp = s.pllNoiseBandwidth;
    double T = 0.001; // Default integration step (1 ms)

    // 3rd order Costas loop coefficients
    double wn = Bp / 0.7845;
    pf1 = 2.4 * wn * T;
    pf2 = 1.1 * wn * wn * T * T;
    pf3 = 0.5 * std::pow(wn * T, 3);
}

std::vector<ChannelTrackResult> Tracking::run(std::ifstream& fid, std::vector<Channel>& channels, const Settings& settings)
{
    int codePeriods = settings.msToProcess;

    std::vector<ChannelTrackResult> trackResults(settings.numberOfChannels);

    double taulcode = 0.0, tau2code = 0.0;
    calcLoopCoef(settings.dllNoiseBandwidth, settings.dllDampingRatio, 1.0, taulcode, tau2code);

    double pf3 = 0.0, pf2 = 0.0, pf1 = 0.0;
    calcLoopCoefCarr(settings, pf3, pf2, pf1);

    for (int channelNr = 0; channelNr < settings.numberOfChannels; ++channelNr)
    {
        if (channels[channelNr].PRN == 0) continue;

        int prn = channels[channelNr].PRN;
        ChannelTrackResult& res = trackResults[channelNr];
        res.PRN = prn;
        res.status = 1;

        // Allocate Data Correlators
        res.absoluteSample.assign(codePeriods, 0.0);
        res.codeFreq.assign(codePeriods, 0.0);
        res.carrFreq.assign(codePeriods, 0.0);
        res.I_E.assign(codePeriods, 0.0);
        res.I_P.assign(codePeriods, 0.0);
        res.I_L.assign(codePeriods, 0.0);
        res.Q_E.assign(codePeriods, 0.0);
        res.Q_P.assign(codePeriods, 0.0);
        res.Q_L.assign(codePeriods, 0.0);

        // Allocate Pilot Correlators
        res.Pilot_I_E.assign(codePeriods, 0.0);
        res.Pilot_I_P.assign(codePeriods, 0.0);
        res.Pilot_I_L.assign(codePeriods, 0.0);
        res.Pilot_Q_E.assign(codePeriods, 0.0);
        res.Pilot_Q_P.assign(codePeriods, 0.0);
        res.Pilot_Q_L.assign(codePeriods, 0.0);

        res.dllError.assign(codePeriods, 0.0);
        res.pllError.assign(codePeriods, 0.0);
        res.CNo.assign(codePeriods, 35.0);
    }

    return trackResults;
}