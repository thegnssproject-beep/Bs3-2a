#pragma once

#include <QString>
#include <memory>
#include <atomic>

struct Settings
{
    // Optional cooperative-cancellation flag set by the GUI (e.g. on a Cancel
    // button or window close). The pipeline polls it between long-running
    // stages/channels and stops promptly. Null when unused.
    std::shared_ptr<std::atomic<bool>> cancelRequested;

    // Speed of Light Constant
    double c = 299792458.0;

    // File Parameters
    QString fileName = "";
    QString dataType = "schar";
    int fileType = 1; // 1 = Real, 2 = IQ
    long long msToProcess = 49000;
    int numberOfChannels = 12;
    long long skipNumberOfBytes = 0;
    double IF = 13.55e6;
    double samplingFreq = 99.375e6;

    // Acquisition Parameters
    bool skipAcquisition = false;
    QString acqSatelliteList = "19 20";

    // Satellite Allocation Configuration:
    // 0: Auto (All above threshold), 1: Specific PRN List (e.g. "19 20 30"),
    // 2: PRN Range (e.g. "10-30" or "19:25"), 3: Top-N Strongest (e.g. "4")
    int satSelectMode = 0;
    QString targetSatList = "19 20 30 37";
    int maxChannels = 12;

    double acqSearchBand = 5000.0;
    double acqThreshold = 1.5;
    double acqStep = 400.0;
    int fineNoncoh = 15;
    bool resamplingflag = false;
    int codeLength = 10230;
    double codeFreqBasis = 10.23e6;

    // Tracking Parameters
    bool pilotTRKflag = true;
    double dllDampingRatio = 0.7;
    double dllNoiseBandwidth = 2.0;
    double pllDampingRatio = 0.7;
    double pllNoiseBandwidth = 20.0;
    double intTime = 0.001;
    double dllCorrelatorSpacing = 0.5;

    // Quality & Navigation Parameters
    int CNoInterval = 1000;
    int navSolPeriod = 500;
    int elevationMask = 5;
    bool useTropCorr = true;
    double startOffset = 0.068802;
};