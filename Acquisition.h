#pragma once
#include <vector>
#include <complex>
#include "Settings.h"

struct AcqResults {
    std::vector<double> carrFreq;
    std::vector<double> codePhase;
    std::vector<double> peakMetric;
};

class Acquisition {
public:
    static std::vector<double> generateB2aDataCode(int prn, const Settings& settings);
    static std::vector<double> generateB2aPilotCode(int prn, const Settings& settings);
    static AcqResults run(const std::vector<std::complex<double>>& signalData, const Settings& settings);
};