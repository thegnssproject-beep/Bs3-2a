#pragma once

#include <vector>
#include <complex>
#include <fstream>
#include <iostream>
#include <cmath>
#include "Settings.h"
#include "Acquisition.h"
#include "PreRun.h"
#include "Tracking.h"
#include "PostNavigation.h"

struct SDRPipelineResults {
    AcqResults acqResults;
    std::vector<ChannelTrackResult> trackResults;
    NavSolutions navSolutions;
};

class PostProcessor {
public:
    static bool runPipeline(const Settings& s, SDRPipelineResults& outResults) {
        std::cout << "[PostProcessor] Starting processing pipeline...\n";

        // 1. Open Binary Signal File
        std::ifstream fid(s.fileName.toStdString(), std::ios::binary);
        if (!fid.is_open()) {
            std::cerr << "[PostProcessor] Error: Unable to open signal file: "
                << s.fileName.toStdString() << "\n";
            return false;
        }

        int dataAdaptCoeff = (s.fileType == 1) ? 1 : 2;
        fid.seekg(dataAdaptCoeff * s.skipNumberOfBytes, std::ios::beg);

        // 2. Prepare Acquisition Data Buffer
        long long samplesPerCode = static_cast<long long>(std::round(
            s.samplingFreq / (s.codeFreqBasis / s.codeLength)
        ));

        long long totalSamplesToRead = samplesPerCode * (s.fineNoncoh + 2);
        long long totalElementsToRead = dataAdaptCoeff * totalSamplesToRead;

        std::vector<int8_t> rawBuffer(totalElementsToRead);
        fid.read(reinterpret_cast<char*>(rawBuffer.data()), totalElementsToRead);
        std::streamsize bytesRead = fid.gcount();

        if (bytesRead < totalElementsToRead) {
            std::cerr << "[PostProcessor] Warning: Reached end of file early during acquisition buffer read.\n";
            if (bytesRead <= 0) return false;
            totalSamplesToRead = bytesRead / dataAdaptCoeff;
        }

        std::vector<std::complex<double>> acqData(totalSamplesToRead);
        if (dataAdaptCoeff == 1) {
            for (long long i = 0; i < totalSamplesToRead; ++i) {
                acqData[i] = std::complex<double>(static_cast<double>(rawBuffer[i]), 0.0);
            }
        }
        else {
            for (long long i = 0; i < totalSamplesToRead; ++i) {
                double I = static_cast<double>(rawBuffer[2 * i]);
                double Q = static_cast<double>(rawBuffer[2 * i + 1]);
                acqData[i] = std::complex<double>(I, Q);
            }
        }

        // 3. Cold Start Acquisition
        if (!s.skipAcquisition) {
            std::cout << "[PostProcessor] Running acquisition search across PRNs...\n";
            outResults.acqResults = Acquisition::run(acqData, s);
        }

        // 4. Channel Allocation (PreRun)
        std::vector<Channel> channels = PreRun::initChannels(outResults.acqResults, s);
        PreRun::showChannelStatus(channels);

        int activeCount = 0;
        for (const auto& ch : channels) {
            if (ch.PRN > 0) activeCount++;
        }

        if (activeCount == 0) {
            std::cerr << "[PostProcessor] Notice: No satellites acquired above threshold.\n";
            fid.close();
            // Return true so the user can still see the Acquisition peak plot
            return true;
        }

        // 5. Code & Carrier Tracking Loops (DLL/PLL)
        std::cout << "[PostProcessor] Running tracking loops on " << activeCount << " channels...\n";
        outResults.trackResults = Tracking::run(fid, channels, s);
        fid.close();

        // 6. Navigation Fix Calculation (PVT)
        std::cout << "[PostProcessor] Running navigation solution (PVT)...\n";
        bool navSuccess = PostNavigation::run(outResults.trackResults, s, outResults.navSolutions);
        if (!navSuccess) {
            std::cerr << "[PostProcessor] Warning: Navigation solution could not resolve complete 3D fix (need >= 4 decoded SVs).\n";
        }

        std::cout << "[PostProcessor] Post-processing pipeline complete!\n";
        return true;
    }
};