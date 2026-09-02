#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

#include "PostNavigation.h"
#include "CommonBS2.h"
#include "Settings.h"
#include "Tracking.h"
#include "Ephemeris.h"
#include "EphemerisDecoder.h"
#include "Satpos.h"

#include <cmath>
#include <iostream>
#include <algorithm>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void writeDiag(const std::string& line) {
    std::ofstream out("C:/Yousuf_B2a/BS-3-2a_GUI/nav_diag.txt", std::ios::app);
    out << line << "\n";
    out.flush();
}

bool PostNavigation::run(const std::vector<ChannelTrackResult>& trackResults,
    const Settings& settings,
    NavSolutions& navSolutions)
{
    writeDiag("PostNavigation::run called with " + std::to_string(trackResults.size()) + " trackResults");
    navSolutions.activePrns.clear();
    navSolutions.azimuth.clear();
    navSolutions.elevation.clear();
    navSolutions.latitude.clear();
    navSolutions.longitude.clear();
    navSolutions.height.clear();
    navSolutions.X.clear();
    navSolutions.Y.clear();
    navSolutions.Z.clear();
    navSolutions.dt.clear();
    navSolutions.HDOP.clear();
    navSolutions.VDOP.clear();
    navSolutions.localTime.clear();
    navSolutions.currMeasSample.clear();

    if (trackResults.empty()) return false;

    std::vector<int> channelList;
    for (size_t ch = 0; ch < trackResults.size(); ++ch) {
        if (trackResults[ch].PRN > 0 && trackResults[ch].status != 0 && !trackResults[ch].I_P.empty()) {
            channelList.push_back(static_cast<int>(ch));
            navSolutions.activePrns.push_back(trackResults[ch].PRN);
        }
    }

    if (channelList.size() < 4) {
        return false;
    }

    size_t numSats = channelList.size();
    int navPeriodMs = (settings.navSolPeriod > 0) ? settings.navSolPeriod : 500;
    int totalMs = static_cast<int>(trackResults[channelList[0]].I_P.size());
    int navEpochs = std::max(1, totalMs / navPeriodMs);

    navSolutions.latitude.resize(navEpochs, 0.0);
    navSolutions.longitude.resize(navEpochs, 0.0);
    navSolutions.height.resize(navEpochs, 0.0);
    navSolutions.X.resize(navEpochs, 0.0);
    navSolutions.Y.resize(navEpochs, 0.0);
    navSolutions.Z.resize(navEpochs, 0.0);
    navSolutions.dt.resize(navEpochs, 0.0);
    navSolutions.HDOP.resize(navEpochs, 0.0);
    navSolutions.VDOP.resize(navEpochs, 0.0);
    navSolutions.localTime.resize(navEpochs, 0.0);
    navSolutions.currMeasSample.resize(navEpochs, 0);

    navSolutions.azimuth.assign(numSats, std::vector<double>(navEpochs, 0.0));
    navSolutions.elevation.assign(numSats, std::vector<double>(navEpochs, 0.0));

    int numChannels = (settings.numberOfChannels > 0) ? settings.numberOfChannels : static_cast<int>(trackResults.size());
    std::vector<Ephemeris> eph(numChannels);
    std::vector<int> subFrameStart(numChannels, 0);
    std::vector<double> TOW(numChannels, 0.0);

    for (int ch : channelList) {
        if (settings.cancelRequested && settings.cancelRequested->load()) return false;

        eph[ch].PRN = trackResults[ch].PRN;
        int frameStart = 0;
        double towVal = 0.0;
        if (EphemerisDecoder::findAllEphemerisFrames(trackResults[ch].I_P, eph[ch], frameStart, towVal)) {
            subFrameStart[ch] = frameStart;
            TOW[ch] = towVal;
            writeDiag("EPHEM PRN=" + std::to_string(eph[ch].PRN) + " OK: idValid=" +
                std::to_string(eph[ch].idValid[0]) + "," +
                std::to_string(eph[ch].idValid[1]) + "," +
                std::to_string(eph[ch].idValid[2]) + "," +
                std::to_string(eph[ch].idValid[3]) + "," +
                std::to_string(eph[ch].idValid[4]) + "," +
                std::to_string(eph[ch].idValid[5]) + "," +
                std::to_string(eph[ch].idValid[6]) + "," +
                std::to_string(eph[ch].idValid[7]) +
                " t_oe=" + std::to_string(eph[ch].t_oe) +
                " t_oc=" + std::to_string(eph[ch].t_oc));
        } else {
            writeDiag("EPHEM PRN=" + std::to_string(eph[ch].PRN) + " FAILED: idValid=" +
                std::to_string(eph[ch].idValid[0]) + "," +
                std::to_string(eph[ch].idValid[1]) + "," +
                std::to_string(eph[ch].idValid[2]) + "," +
                std::to_string(eph[ch].idValid[3]) + "," +
                std::to_string(eph[ch].idValid[4]) + "," +
                std::to_string(eph[ch].idValid[5]) + "," +
                std::to_string(eph[ch].idValid[6]) + "," +
                std::to_string(eph[ch].idValid[7]));
        }
    }

    double localTime = 1e30;
    std::vector<double> userPos = { 0.0, 0.0, 0.0, 0.0 };
    bool anyFix = false;

    // Per-satellite elevation (degrees) used for the elevation mask, indexed by
    // position in activePrns. Initialized above the mask so the first fix
    // includes all ready satellites (matches MATLAB postNavigation.m satElev init).
    std::vector<double> satElev(numSats, 100.0);

    for (int ep = 0; ep < navEpochs; ++ep) {
        if (settings.cancelRequested && settings.cancelRequested->load()) break;

        int sampleIdx = ep * navPeriodMs;
        double currMeasSample = static_cast<double>(sampleIdx) * (settings.samplingFreq / 1000.0);

        navSolutions.localTime[ep] = sampleIdx / 1000.0;
        navSolutions.currMeasSample[ep] = sampleIdx;

        // Exclude satellites below the elevation mask (MATLAB postNavigation.m:164)
        std::vector<int> activeSubset;
        for (int i = 0; i < static_cast<int>(channelList.size()); ++i) {
            if (satElev[i] >= settings.elevationMask) {
                activeSubset.push_back(i);
            }
        }

        std::vector<double> pseudoranges(numChannels, 0.0);
        std::vector<double> transmitTime(numChannels, 1e30);

        CommonBS2::calculatePseudoranges(
            trackResults,
            subFrameStart,
            TOW,
            currMeasSample,
            localTime,
            channelList,
            settings,
            pseudoranges,
            transmitTime
        );

        std::vector<double> obsP;
        std::vector<int> prnsForFix;
        std::vector<double> txTimes;
        std::vector<int> fixRows;

        for (int idx : activeSubset) {
            int ch = channelList[idx];
            if (ch < numChannels && transmitTime[ch] < 1e20 && pseudoranges[ch] > 1e4) {
                obsP.push_back(pseudoranges[ch]);
                prnsForFix.push_back(trackResults[ch].PRN);
                txTimes.push_back(transmitTime[ch]);
                fixRows.push_back(idx);
            }
        }

        if (obsP.size() < 4) {
            continue;
        }

        // Faithful satpos (IGSO/GEO reference, T_GDB1Cp, n0 from A0, clock-corrected tk).
        // Returns positions in axis-major layout [3][numSats].
        std::vector<std::vector<double>> satPosAxis(3, std::vector<double>(obsP.size(), 0.0));
        std::vector<double> satClkCorr(obsP.size(), 0.0);

        Satpos::computePositions(txTimes, prnsForFix, eph, satPosAxis, satClkCorr);

        // Transpose to per-satellite rows [sat][xyz] required by leastSquarePos
        // and add satellite clock corrections to the raw pseudoranges.
        std::vector<std::vector<double>> satPositions(obsP.size(), std::vector<double>(3, 0.0));
        for (size_t s = 0; s < obsP.size(); ++s) {
            satPositions[s][0] = satPosAxis[0][s];
            satPositions[s][1] = satPosAxis[1][s];
            satPositions[s][2] = satPosAxis[2][s];
            obsP[s] += satClkCorr[s] * settings.c;
        }

        std::vector<double> pos(4, 0.0);
        std::vector<double> el(obsP.size(), 0.0);
        std::vector<double> az(obsP.size(), 0.0);
        std::vector<double> dop(5, 0.0);

        if (CommonBS2::leastSquarePos(satPositions, obsP, settings, pos, el, az, dop)) {
            anyFix = true;
            userPos = pos;

            double lat = 0.0, lon = 0.0, h = 0.0;
            CommonBS2::cart2geo(userPos[0], userPos[1], userPos[2], 5, lat, lon, h);

            navSolutions.X[ep] = userPos[0];
            navSolutions.Y[ep] = userPos[1];
            navSolutions.Z[ep] = userPos[2];

            // Receiver clock error is forced to zero for the first fix
            // (MATLAB postNavigation.m:227)
            navSolutions.dt[ep] = (ep == 0) ? 0.0 : userPos[3];

            navSolutions.latitude[ep] = lat;
            navSolutions.longitude[ep] = lon;
            navSolutions.height[ep] = h;

            navSolutions.HDOP[ep] = dop[2];
            navSolutions.VDOP[ep] = dop[3];

for (size_t j = 0; j < fixRows.size(); ++j) {
            int row = activeSubset[fixRows[j]];
            navSolutions.azimuth[row][ep] = az[j];
            navSolutions.elevation[row][ep] = el[j];
            satElev[row] = el[j];
        }

            // Correct local time by the estimated receiver clock error
            // (MATLAB postNavigation.m:232)
            localTime -= userPos[3] / settings.c;
        }

        // Advance local time by the measurement sample step
        // (MATLAB postNavigation.m:302)
        localTime += navPeriodMs / 1000.0;
    }

    // Best-effort sky positions: when no receiver position fix is possible
    // (typically fewer than 4 satellites with usable ephemeris), still place each
    // decoded satellite on the sky plot using its ephemeris-derived ECEF position
    // relative to a nominal geodetic receiver reference. Without a configured
    // receiver location this azimuth/elevation is approximate (labelled "geo"),
    // but it uses the real ephemeris geometry to spread PRNs across the sky
    // instead of stacking every PRN at the zenith. The geocentric (Earth-centre)
    // reference is degenerate for this purpose, so a nominal surface location
    // is used here and can be replaced once a receiver position is available.
    if (!anyFix) {
        std::vector<int> ephPrns;
        std::vector<int> ephRows;
        for (int idx = 0; idx < static_cast<int>(channelList.size()); ++idx) {
            int ch = channelList[idx];
            bool valid10 = (ch < (int)eph.size() && eph[ch].idValid.size() >= 2 &&
                            eph[ch].idValid[0] == 10 && eph[ch].idValid[1] == 11);
            bool hasClock = false;
            if (ch < (int)eph.size() && eph[ch].idValid.size() >= 3) {
                for (int k = 2; k < (int)eph[ch].idValid.size(); ++k)
                    if (eph[ch].idValid[k] >= 30 && eph[ch].idValid[k] <= 34) hasClock = true;
            }
            if (valid10 && hasClock) {
                ephPrns.push_back(trackResults[ch].PRN);
                ephRows.push_back(idx);
            }
        }

        if (!ephPrns.empty()) {
            // Use each satellite's own ephemeris reference epoch as transmit time,
            // giving a valid ECEF position for the constellation geometry.
            std::vector<double> txTimes(ephPrns.size(), 0.0);
            for (size_t s = 0; s < ephPrns.size(); ++s) {
                int ch = channelList[ephRows[s]];
                txTimes[s] = (ch < (int)eph.size()) ? eph[ch].t_oe : 0.0;
            }

            std::vector<std::vector<double>> satPosAxis(3, std::vector<double>(ephPrns.size(), 0.0));
            std::vector<double> satClk(ephPrns.size(), 0.0);
            Satpos::computePositions(txTimes, ephPrns, eph, satPosAxis, satClk);

            // Nominal geodetic receiver reference (approx). Convert to ECEF so
            // azimuth/elevation are computed from a valid surface observer point.
            const double refLat = 30.0 * M_PI / 180.0;
            const double refLon = 110.0 * M_PI / 180.0;
            const double refH = 0.0;
            const double a_wgs = 6378137.0;
            const double e2_wgs = 0.00669437999014;
            const double Nref = a_wgs / std::sqrt(1.0 - e2_wgs * std::sin(refLat) * std::sin(refLat));
            std::vector<double> refEcef = {
                (Nref + refH) * std::cos(refLat) * std::cos(refLon),
                (Nref + refH) * std::cos(refLat) * std::sin(refLon),
                (Nref * (1.0 - e2_wgs) + refH) * std::sin(refLat)
            };

            for (size_t s = 0; s < ephPrns.size(); ++s) {
                std::vector<double> diff = {
                    satPosAxis[0][s] - refEcef[0],
                    satPosAxis[1][s] - refEcef[1],
                    satPosAxis[2][s] - refEcef[2]
                };
                double elDeg = 0.0, azDeg = 0.0, dist = 0.0;
                CommonBS2::topocent(refEcef, diff, azDeg, elDeg, dist);
                int row = ephRows[s];
                for (int ep = 0; ep < navEpochs; ++ep) {
                    navSolutions.azimuth[row][ep] = azDeg;
                    navSolutions.elevation[row][ep] = elDeg;
                }
                writeDiag("SKY PRN=" + std::to_string(ephPrns[s]) + " az=" +
                    std::to_string(azDeg) + " el=" + std::to_string(elDeg));
            }
        }
    }

    return true;
}

void PostNavigation::cart2geo(double X, double Y, double Z,
    double& lat, double& lon, double& h) noexcept
{
    CommonBS2::cart2geo(X, Y, Z, 5, lat, lon, h);
}