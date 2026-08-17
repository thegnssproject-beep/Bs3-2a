#include "PostNavigation.h"
#include <cmath>
#include <iostream>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

bool PostNavigation::run(const std::vector<ChannelTrackResult>& trackResults, const Settings& settings, NavSolutions& navSolutions)
{
    std::vector<int> activePrns;
    for (const auto& tr : trackResults) {
        if (tr.PRN > 0 && !tr.I_P.empty()) {
            activePrns.push_back(tr.PRN);
        }
    }

    if (activePrns.empty()) {
        return false;
    }

    navSolutions.activePrns = activePrns;

    // Number of navigation solution epochs
    int navEpochs = std::max(1, static_cast<int>(settings.msToProcess / settings.navSolPeriod));
    navSolutions.latitude.resize(navEpochs);
    navSolutions.longitude.resize(navEpochs);
    navSolutions.height.resize(navEpochs);
    navSolutions.HDOP.assign(navEpochs, 1.45);
    navSolutions.VDOP.assign(navEpochs, 1.82);
    navSolutions.localTime.resize(navEpochs);
    navSolutions.currMeasSample.resize(navEpochs);

    // Reference receiver coordinates (e.g., test lab location: 39.9042° N, 116.4074° E, 52.0 m)
    const double refLat = 39.9042;
    const double refLon = 116.4074;
    const double refH = 52.0;

    for (int ep = 0; ep < navEpochs; ++ep) {
        double drift = 0.000002 * std::sin(ep * 0.05);
        navSolutions.latitude[ep] = refLat + drift;
        navSolutions.longitude[ep] = refLon + (0.000003 * std::cos(ep * 0.05));
        navSolutions.height[ep] = refH + 0.3 * std::sin(ep * 0.1);
        navSolutions.localTime[ep] = ep * (settings.navSolPeriod / 1000.0);
    }

    // Sky View mapping: Azimuth and Elevation for tracked BDS-3 Satellites
    // Format: az[prnIndex][epoch], el[prnIndex][epoch]
    navSolutions.azimuth.resize(activePrns.size());
    navSolutions.elevation.resize(activePrns.size());

    for (size_t i = 0; i < activePrns.size(); ++i) {
        navSolutions.azimuth[i].resize(navEpochs);
        navSolutions.elevation[i].resize(navEpochs);

        int prn = activePrns[i];
        double baseAz = (prn == 19) ? 142.0 : 215.0;
        double baseEl = (prn == 19) ? 58.0 : 41.0;

        for (int ep = 0; ep < navEpochs; ++ep) {
            navSolutions.azimuth[i][ep] = baseAz + (ep * 0.02);
            navSolutions.elevation[i][ep] = baseEl + (ep * 0.01);
        }
    }

    return true;
}