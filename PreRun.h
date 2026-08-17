#pragma once

#include <vector>
#include <string>
#include <algorithm>
#include <numeric>
#include <iostream>
#include "Settings.h"
#include "Acquisition.h"

struct Channel
{
    int PRN = 0;              // PRN number of the tracked satellite
    double acquiredFreq = 0.0; // Center frequency for NCO (Hz)
    double codePhase = 0.0;    // Position of code start in samples
    double codeFreq = 0.0;     // Initial frequency for code NCO (Hz)
    char status = '-';         // '-' = Off, 'T' = Tracking
};

class PreRun
{
public:
    static std::vector<Channel> initChannels(const AcqResults& acqResults, const Settings& settings)
    {
        // 1. Initialize channel array with default off state
        std::vector<Channel> channels(settings.numberOfChannels);

        // 2. Extract valid PRN indices with detected carrier frequencies
        std::vector<int> acquiredPRNs;
        for (size_t prn = 1; prn < acqResults.carrFreq.size(); ++prn) {
            if (acqResults.carrFreq[prn] != 0.0) {
                acquiredPRNs.push_back(static_cast<int>(prn));
            }
        }

        if (acquiredPRNs.empty()) {
            std::cout << "[PreRun] No active satellites detected for tracking allocation.\n";
            return channels;
        }

        // 3. Sort PRNs in descending order of their peak metrics (signal strength)
        std::sort(acquiredPRNs.begin(), acquiredPRNs.end(), [&acqResults](int a, int b) {
            return acqResults.peakMetric[a] > acqResults.peakMetric[b];
            });

        // 4. Assign top acquired signals up to numberOfChannels
        size_t channelsToInit = std::min(static_cast<size_t>(settings.numberOfChannels), acquiredPRNs.size());

        for (size_t i = 0; i < channelsToInit; ++i) {
            int prn = acquiredPRNs[i];
            channels[i].PRN = prn;
            channels[i].acquiredFreq = acqResults.carrFreq[prn];
            channels[i].codePhase = acqResults.codePhase[prn];
            channels[i].codeFreq = settings.codeFreqBasis;
            channels[i].status = 'T'; // Set status to Active Tracking
        }

        return channels;
    }

    static void showChannelStatus(const std::vector<Channel>& channels)
    {
        std::cout << "\n=================== Channel Status Allocation ===================\n";
        std::cout << "Channel | Status | PRN | Acquired Freq (Hz) | Code Phase (samples)\n";
        std::cout << "--------+--------+-----+--------------------+---------------------\n";
        for (size_t i = 0; i < channels.size(); ++i) {
            std::cout << "   " << (i + 1) << "    |   "
                << channels[i].status << "    | "
                << (channels[i].PRN < 10 ? " " : "") << channels[i].PRN << "  | "
                << channels[i].acquiredFreq << "       | "
                << channels[i].codePhase << "\n";
        }
        std::cout << "=================================================================\n\n";
    }
};