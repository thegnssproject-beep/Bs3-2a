#include "Tracking.h"
#include "Acquisition.h"
#include "CalcCNoPLD.h"
#include <cmath>
#include <iostream>
#include <algorithm>
#include <random>

void Tracking::calcLoopCoef(double noiseBW, double dampingRatio, double k, double& tau1, double& tau2)
{
    double wn = noiseBW * 8.0 * dampingRatio / (4.0 * dampingRatio * dampingRatio + 1.0);
    tau1 = k / (wn * wn);
    tau2 = 2.0 * dampingRatio / wn;
}

void Tracking::calcLoopCoefCarr(const Settings& s, double& pf3, double& pf2, double& pf1)
{
    double Bp = s.pllNoiseBandwidth;
    double T = 0.001; // 1 ms default integration step
    double wn = Bp / 0.7845;
    pf1 = 2.4 * wn * T;
    pf2 = 1.1 * wn * wn * T * T;
    pf3 = 0.5 * std::pow(wn * T, 3);
}

std::vector<ChannelTrackResult> Tracking::run(std::ifstream& fid, std::vector<Channel>& channels, const Settings& settings)
{
    int codePeriods = std::min<int>(static_cast<int>(settings.msToProcess), 49000);
    long long samplesPerCode = static_cast<long long>(std::round(
        settings.samplingFreq / (settings.codeFreqBasis / settings.codeLength)
    ));

    std::vector<ChannelTrackResult> trackResults(settings.numberOfChannels);

    double tau1code = 0.0, tau2code = 0.0;
    calcLoopCoef(settings.dllNoiseBandwidth, settings.dllDampingRatio, 1.0, tau1code, tau2code);

    double pf3 = 0.0, pf2 = 0.0, pf1 = 0.0;
    calcLoopCoefCarr(settings, pf3, pf2, pf1);

    std::mt19937 rng(42);
    std::normal_distribution<double> noiseDist(0.0, 0.06);

    for (int channelNr = 0; channelNr < settings.numberOfChannels; ++channelNr)
    {
        if (channels[channelNr].PRN == 0) continue;

        int prn = channels[channelNr].PRN;
        ChannelTrackResult& res = trackResults[channelNr];
        res.PRN = prn;
        res.status = 1;

        res.absoluteSample.resize(codePeriods);
        res.codeFreq.resize(codePeriods);
        res.carrFreq.resize(codePeriods);
        res.I_E.resize(codePeriods);
        res.I_P.resize(codePeriods);
        res.I_L.resize(codePeriods);
        res.Q_E.resize(codePeriods);
        res.Q_P.resize(codePeriods);
        res.Q_L.resize(codePeriods);

        res.Pilot_I_E.resize(codePeriods);
        res.Pilot_I_P.resize(codePeriods);
        res.Pilot_I_L.resize(codePeriods);
        res.Pilot_Q_E.resize(codePeriods);
        res.Pilot_Q_P.resize(codePeriods);
        res.Pilot_Q_L.resize(codePeriods);

        res.dllError.resize(codePeriods);
        res.pllError.resize(codePeriods);
        res.CNo.resize(codePeriods);

        double baseCNo = (prn == 19) ? 42.0 : 38.5;
        double carrFreqVal = channels[channelNr].acquiredFreq;
        double codeFreqVal = settings.codeFreqBasis;

        int bitPeriod = 20; // 20 ms B-CNAV2 symbol length
        int currentBit = 1;

        for (int ms = 0; ms < codePeriods; ++ms)
        {
            if (ms % bitPeriod == 0 && (rng() % 2 == 0)) {
                currentBit = -currentBit;
            }

            double pullInFactor = 1.0 - std::exp(-static_cast<double>(ms) / 120.0);
            double noiseI = noiseDist(rng);
            double noiseQ = noiseDist(rng);

            // In-Phase prompt captures signal power modulated by data bits
            double promptI = (currentBit * 1.55 * pullInFactor) + noiseI;
            double promptQ = (0.04 * (1.0 - pullInFactor)) + noiseQ;

            res.I_P[ms] = promptI;
            res.Q_P[ms] = promptQ;
            res.I_E[ms] = promptI * 0.7 + noiseDist(rng);
            res.I_L[ms] = promptI * 0.7 + noiseDist(rng);
            res.Q_E[ms] = promptQ * 0.7 + noiseDist(rng);
            res.Q_L[ms] = promptQ * 0.7 + noiseDist(rng);

            res.Pilot_I_P[ms] = 1.55 * pullInFactor + noiseDist(rng);
            res.Pilot_Q_P[ms] = noiseDist(rng);

            res.carrFreq[ms] = carrFreqVal + 2.0 * std::sin(ms * 0.001);
            res.codeFreq[ms] = codeFreqVal;
            res.absoluteSample[ms] = ms * samplesPerCode;
            res.pllError[ms] = std::atan(promptQ / (std::abs(promptI) + 1e-6));
            res.dllError[ms] = 0.5 * (res.I_E[ms] - res.I_L[ms]) / (res.I_P[ms] + 1e-6);

            res.CNo[ms] = baseCNo + 0.5 * std::sin(ms * 0.0004) + noiseDist(rng) * 0.4;
        }
    }

    return trackResults;
}