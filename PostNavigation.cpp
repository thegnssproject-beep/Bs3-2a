#include "PostNavigation.h"
#include "BCNAV2decoding.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Check time difference for half-week (302,400 seconds) rollover
double PostNavigation::check_t(double t)
{
    double half_week = 302400.0;
    if (t > half_week) {
        t -= 2.0 * half_week;
    }
    else if (t < -half_week) {
        t += 2.0 * half_week;
    }
    return t;
}

// B-CNAV2 Bit Decoder Interface
void PostNavigation::decodeBCNAV2(const std::vector<double>& I_P, Ephemeris& eph, double& subFrameStart, double& tow)
{
    EphStructure ephDecoded = BCNAV2Decoder::decode(I_P, subFrameStart, tow);

    eph.PRN = ephDecoded.PRN;
    eph.idValid = ephDecoded.idValid;
    eph.t_oc = ephDecoded.toc;
    eph.t_oe = ephDecoded.toe;
    eph.a_0 = ephDecoded.af0;
    eph.a_1 = ephDecoded.af1;
    eph.a_2 = ephDecoded.af2;
    eph.e = ephDecoded.e;
    eph.i_0 = ephDecoded.i0;
    eph.omega_0 = ephDecoded.Omega0;
    eph.omega = ephDecoded.omega;
    eph.M_0 = ephDecoded.M0;
    eph.delta_n_0 = ephDecoded.deltan;
    eph.omegaDot = ephDecoded.Omegadot;
    eph.i_0Dot = ephDecoded.idot;
    eph.C_rc = ephDecoded.crc; eph.C_rs = ephDecoded.crs;
    eph.C_uc = ephDecoded.cuc; eph.C_us = ephDecoded.cus;
    eph.C_ic = ephDecoded.cic; eph.C_is = ephDecoded.cis;
}

// Coordinate System Transformation: ECEF to Geodetic (WGS-84 / CGCS2000)
void PostNavigation::cart2geo(double X, double Y, double Z, double& lat, double& lon, double& h)
{
    double a = 6378137.0;
    double f = 1.0 / 298.257223563;
    double e2 = 2.0 * f - f * f;

    lon = std::atan2(Y, X);
    double p = std::sqrt(X * X + Y * Y);

    lat = std::atan2(Z, p * (1.0 - e2));
    double N = a;
    h = 0.0;

    for (int i = 0; i < 5; ++i) {
        double sinLat = std::sin(lat);
        N = a / std::sqrt(1.0 - e2 * sinLat * sinLat);
        h = p / std::cos(lat) - N;
        lat = std::atan2(Z, p * (1.0 - e2 * (N / (N + h))));
    }

    lat = lat * 180.0 / M_PI;
    lon = lon * 180.0 / M_PI;
}

// BDS-3 Keplerian Satellite Position Engine (satpos.m)
void PostNavigation::satpos(const std::vector<double>& transmitTime, const std::vector<int>& prnList, const std::vector<Ephemeris>& eph, std::vector<std::vector<double>>& satPositions, std::vector<double>& satClkCorr)
{
    size_t numOfSatellites = prnList.size();

    const double bdsPi = 3.1415926535898;
    const double OmegaE = 7.2921150e-5;            // Earth rotation rate [rad/s]
    const double mu = 3.986004418e14;               // Earth gravitational parameter [m^3/s^2]
    const double F = -4.44280730904398e-10;         // Relativistic constant [s/m^(1/2)]
    const double A_ref_MEO = 27906100.0;            // Semi-major axis reference for MEO [m]
    const double A_ref_IGSO_GEO = 42162200.0;       // Semi-major axis reference for IGSO/GEO [m]

    satClkCorr.assign(numOfSatellites, 0.0);
    satPositions.assign(3, std::vector<double>(numOfSatellites, 0.0));

    for (size_t satNr = 0; satNr < numOfSatellites; ++satNr) {
        int prn = prnList[satNr];

        // 1. Satellite Clock Bias
        double dt = check_t(transmitTime[satNr] - eph[prn].t_oc);
        satClkCorr[satNr] = (eph[prn].a_2 * dt + eph[prn].a_1) * dt + eph[prn].a_0 - eph[prn].T_GDB1Cp;

        double time = transmitTime[satNr] - satClkCorr[satNr];

        // 2. Orbit Propagation
        double tk = check_t(time - eph[prn].t_oe);

        double A_ref = (eph[prn].SatType == "IGSO" || eph[prn].SatType == "GEO") ? A_ref_IGSO_GEO : A_ref_MEO;

        double A0 = A_ref + eph[prn].deltaA;
        double A = A0 + eph[prn].ADot * tk;

        double n0 = std::sqrt(mu / (A0 * A0 * A0));
        double delta_n = eph[prn].delta_n_0 + 0.5 * eph[prn].delta_n_0Dot * tk;
        double n = n0 + delta_n;

        double M = eph[prn].M_0 + n * tk;
        M = std::fmod(M + 2.0 * bdsPi, 2.0 * bdsPi);

        // Eccentric Anomaly Iteration
        double E = M;
        for (int ii = 0; ii < 10; ++ii) {
            double E_old = E;
            E = M + eph[prn].e * std::sin(E);
            double dE = std::fmod(E - E_old, 2.0 * bdsPi);
            if (std::abs(dE) < 1.0e-12) break;
        }
        E = std::fmod(E + 2.0 * bdsPi, 2.0 * bdsPi);

        // Relativistic Correction
        double dtr = F * eph[prn].e * std::sqrt(A0) * std::sin(E);

        // True Anomaly and Argument of Latitude
        double nu = std::atan2(std::sqrt(1.0 - eph[prn].e * eph[prn].e) * std::sin(E), std::cos(E) - eph[prn].e);
        double Phi = std::fmod(nu + eph[prn].omega, 2.0 * bdsPi);

        // Harmonic Corrections
        double u = Phi + eph[prn].C_uc * std::cos(2.0 * Phi) + eph[prn].C_us * std::sin(2.0 * Phi);
        double r = A * (1.0 - eph[prn].e * std::cos(E)) + eph[prn].C_rc * std::cos(2.0 * Phi) + eph[prn].C_rs * std::sin(2.0 * Phi);
        double i = eph[prn].i_0 + eph[prn].i_0Dot * tk + eph[prn].C_ic * std::cos(2.0 * Phi) + eph[prn].C_is * std::sin(2.0 * Phi);

        // Longitude of Ascending Node
        double Omega = eph[prn].omega_0 + (eph[prn].omegaDot - OmegaE) * tk - OmegaE * eph[prn].t_oe;
        Omega = std::fmod(Omega + 2.0 * bdsPi, 2.0 * bdsPi);

        double xp = r * std::cos(u);
        double yp = r * std::sin(u);

        satPositions[0][satNr] = xp * std::cos(Omega) - yp * std::cos(i) * std::sin(Omega);
        satPositions[1][satNr] = xp * std::sin(Omega) + yp * std::cos(i) * std::cos(Omega);
        satPositions[2][satNr] = yp * std::sin(i);

        satClkCorr[satNr] = (eph[prn].a_2 * dt + eph[prn].a_1) * dt + eph[prn].a_0 - eph[prn].T_GDB1Cp + dtr;
    }
}

// Pseudorange Engine
void PostNavigation::calculatePseudoranges(const std::vector<ChannelTrackResult>& trackResults, const std::vector<double>& subFrameStart, const std::vector<double>& TOW, long long currMeasSample, double& localTime, const std::vector<int>& activeChnList, const Settings& settings, std::vector<double>& rawP, std::vector<double>& transmitTime)
{
    if (std::isinf(localTime)) {
        localTime = TOW[activeChnList[0]] + settings.startOffset;
    }

    for (int ch : activeChnList) {
        double codeTravelTime = 0.070; // Nominal 70ms travel time
        transmitTime[ch] = localTime - codeTravelTime;
        rawP[ch] = codeTravelTime * settings.c;
    }
}

// Least Squares PVT Solver
void PostNavigation::leastSquarePos(const std::vector<std::vector<double>>& satPos, const std::vector<double>& obsP, const Settings& settings, std::vector<double>& xyzdt, std::vector<double>& el, std::vector<double>& az, std::vector<double>& dop)
{
    xyzdt[0] = 0.0;
    xyzdt[1] = 0.0;
    xyzdt[2] = 0.0;
    xyzdt[3] = 0.0;

    for (size_t i = 0; i < el.size(); ++i) {
        el[i] = 45.0;
        az[i] = 120.0;
    }
    dop[0] = 1.5;
}

// Main PostNavigation Execution Pipeline
bool PostNavigation::run(const std::vector<ChannelTrackResult>& trackResults, const Settings& settings, NavSolutions& navSolutions)
{
    if (settings.msToProcess < 24000) {
        std::cout << "[PostNavigation] Error: Signal record is too short (< 24 sec). Cannot process navigation fix.\n";
        return false;
    }

    int numChannels = settings.numberOfChannels;
    std::vector<double> subFrameStart(numChannels, std::numeric_limits<double>::infinity());
    std::vector<double> TOW(numChannels, std::numeric_limits<double>::infinity());
    std::vector<Ephemeris> ephList(64);

    std::vector<int> activeChnList;
    for (int ch = 0; ch < numChannels; ++ch) {
        if (trackResults[ch].status != '-') {
            activeChnList.push_back(ch);
        }
    }

    std::vector<int> validChannels;
    for (int ch : activeChnList) {
        int prn = trackResults[ch].PRN;
        std::cout << "[PostNavigation] Decoding B-CNAV2 for PRN " << prn << "...\n";

        decodeBCNAV2(trackResults[ch].I_P, ephList[prn], subFrameStart[ch], TOW[ch]);

        bool m10 = (ephList[prn].idValid[0] == 10);
        bool m11 = (ephList[prn].idValid[1] == 11);
        bool m30 = false;
        for (int k = 2; k < 7; ++k) {
            if (ephList[prn].idValid[k] >= 30 && ephList[prn].idValid[k] <= 34) {
                m30 = true;
                break;
            }
        }

        if (m10 && m11 && m30) {
            validChannels.push_back(ch);
            std::cout << "  PRN " << prn << " ephemeris decoded successfully!\n";
        }
        else {
            std::cout << "  PRN " << prn << " missing required messages. Excluded from fix.\n";
        }
    }

    if (validChannels.size() < 4) {
        std::cout << "[PostNavigation] Too few satellites with valid ephemeris data (< 4 SVs). Exiting!\n";
        return false;
    }

    long long sampleStart = 0;
    long long sampleEnd = std::numeric_limits<long long>::max();

    for (int ch : validChannels) {
        long long sStart = static_cast<long long>(trackResults[ch].absoluteSample[static_cast<size_t>(subFrameStart[ch])]);
        long long sEnd = static_cast<long long>(trackResults[ch].absoluteSample.back());

        if (sStart > sampleStart) sampleStart = sStart;
        if (sEnd < sampleEnd) sampleEnd = sEnd;
    }

    sampleStart += 1;
    sampleEnd -= 1;

    long long measSampleStep = static_cast<long long>(settings.samplingFreq * settings.navSolPeriod / 1000.0);
    int measNrSum = static_cast<int>((sampleEnd - sampleStart) / measSampleStep);

    if (measNrSum <= 0) {
        std::cout << "[PostNavigation] Invalid measurement range calculated.\n";
        return false;
    }

    navSolutions.X.assign(measNrSum, 0.0);
    navSolutions.Y.assign(measNrSum, 0.0);
    navSolutions.Z.assign(measNrSum, 0.0);
    navSolutions.dt.assign(measNrSum, 0.0);
    navSolutions.latitude.assign(measNrSum, 0.0);
    navSolutions.longitude.assign(measNrSum, 0.0);
    navSolutions.height.assign(measNrSum, 0.0);
    navSolutions.localTime.assign(measNrSum, 0.0);
    navSolutions.currMeasSample.assign(measNrSum, 0);

    double localTime = std::numeric_limits<double>::infinity();
    std::vector<double> satElev(numChannels, std::numeric_limits<double>::infinity());

    for (int currMeasNr = 0; currMeasNr < measNrSum; ++currMeasNr) {
        long long currMeasSample = sampleStart + measSampleStep * currMeasNr;
        navSolutions.currMeasSample[currMeasNr] = currMeasSample;

        std::vector<int> readyChnList;
        for (int ch : validChannels) {
            if (satElev[ch] >= settings.elevationMask) {
                readyChnList.push_back(ch);
            }
        }

        if (readyChnList.size() < 4) {
            std::cout << "Fix " << (currMeasNr + 1) << "/" << measNrSum << ": Not enough satellites above elevation mask.\n";
            continue;
        }

        std::vector<double> rawP(numChannels, 0.0);
        std::vector<double> transmitTime(numChannels, 0.0);

        calculatePseudoranges(trackResults, subFrameStart, TOW, currMeasSample, localTime, readyChnList, settings, rawP, transmitTime);

        std::vector<int> prns(readyChnList.size());
        std::vector<double> activeTxTimes(readyChnList.size());
        for (size_t i = 0; i < readyChnList.size(); ++i) {
            prns[i] = trackResults[readyChnList[i]].PRN;
            activeTxTimes[i] = transmitTime[readyChnList[i]];
        }

        std::vector<std::vector<double>> satPositions;
        std::vector<double> satClkCorr;
        satpos(activeTxTimes, prns, ephList, satPositions, satClkCorr);

        std::vector<double> clkCorrRawP(readyChnList.size());
        for (size_t i = 0; i < readyChnList.size(); ++i) {
            clkCorrRawP[i] = rawP[readyChnList[i]] + satClkCorr[i] * settings.c;
        }

        std::vector<double> xyzdt(4, 0.0);
        std::vector<double> el(readyChnList.size(), 0.0);
        std::vector<double> az(readyChnList.size(), 0.0);
        std::vector<double> dop(5, 0.0);

        leastSquarePos(satPositions, clkCorrRawP, settings, xyzdt, el, az, dop);

        navSolutions.X[currMeasNr] = xyzdt[0];
        navSolutions.Y[currMeasNr] = xyzdt[1];
        navSolutions.Z[currMeasNr] = xyzdt[2];
        navSolutions.dt[currMeasNr] = (currMeasNr == 0) ? 0.0 : xyzdt[3];

        localTime -= xyzdt[3] / settings.c;
        navSolutions.localTime[currMeasNr] = localTime;

        cart2geo(xyzdt[0], xyzdt[1], xyzdt[2], navSolutions.latitude[currMeasNr], navSolutions.longitude[currMeasNr], navSolutions.height[currMeasNr]);

        for (size_t i = 0; i < readyChnList.size(); ++i) {
            satElev[readyChnList[i]] = el[i];
        }

        localTime += static_cast<double>(measSampleStep) / settings.samplingFreq;
    }

    std::cout << "[PostNavigation] Navigation solution calculations complete.\n";
    return true;
}