#include "Satpos.h"

void Satpos::computePositions(
    const std::vector<double>& transmitTime,
    const std::vector<int>& prnList,
    const std::vector<Ephemeris>& eph,
    std::vector<std::vector<double>>& satPositions,
    std::vector<double>& satClkCorr)
{
    std::vector<std::vector<double>> dummyVel;
    std::vector<double> dummyDrift;
    computePositionsAndVelocities(transmitTime, prnList, eph, satPositions, satClkCorr, dummyVel, dummyDrift);
}

void Satpos::computePositionsAndVelocities(
    const std::vector<double>& transmitTime,
    const std::vector<int>& prnList,
    const std::vector<Ephemeris>& eph,
    std::vector<std::vector<double>>& satPositions,
    std::vector<double>& satClkCorr,
    std::vector<std::vector<double>>& satVelocities,
    std::vector<double>& satClkDrift)
{
    size_t numOfSatellites = prnList.size();

    satClkCorr.assign(numOfSatellites, 0.0);
    satClkDrift.assign(numOfSatellites, 0.0);
    satPositions.assign(3, std::vector<double>(numOfSatellites, 0.0));
    satVelocities.assign(3, std::vector<double>(numOfSatellites, 0.0));

    // BDS / GPS Constants
    const double bdsPi = 3.1415926535898;
    const double OmegaE_dot = 7.2921150e-5;            // Earth rotation rate (rad/s)
    const double GM = 3.986004418e14;          // Gravitational parameter (m^3/s^2)
    const double F = -4.44280730904398e-10;   // Relativistic constant (s/m^(1/2))
    const double A_REF_MEO = 27906100.0;             // Semi-major axis reference MEO (m)
    const double A_REF_IGSO_GEO = 42162200.0;             // Semi-major axis reference IGSO/GEO (m)

    for (size_t satNr = 0; satNr < numOfSatellites; ++satNr) {
        int prn = prnList[satNr];

        if (prn <= 0) {
            continue;
        }

        // Locate the ephemeris for this PRN. The eph vector may be indexed by
        // channel (not by PRN number), so scan for a matching PRN instead of
        // assuming eph[prn] is valid.
        long ephIdx = -1;
        for (size_t e = 0; e < eph.size(); ++e) {
            if (eph[e].PRN == prn) { ephIdx = static_cast<long>(e); break; }
        }
        if (ephIdx < 0) {
            continue;
        }

        const Ephemeris& satEph = eph[static_cast<size_t>(ephIdx)];

        // 1. Satellite Clock Correction
        double dt = CheckT::check(transmitTime[satNr] - satEph.t_oc);

        satClkCorr[satNr] = (satEph.a_2 * dt + satEph.a_1) * dt + satEph.a_0 - satEph.T_GDB1Cp;
        double time = transmitTime[satNr] - satClkCorr[satNr];

        // 2. Ephemeris Reference Time Difference
        double tk = CheckT::check(time - satEph.t_oe);

        double A_REF = A_REF_MEO;
        if (satEph.SatType == "IGSO" || satEph.SatType == "GEO") {
            A_REF = A_REF_IGSO_GEO;
        }

        // Semi-Major Axis
        double A_0 = A_REF + satEph.deltaA;
        double A = A_0 + satEph.ADot * tk;

        // Mean Motion
        double n0 = std::sqrt(GM / (A_0 * A_0 * A_0));
        double delta_n = satEph.delta_n_0 + 0.5 * satEph.delta_n_0Dot * tk;
        double n = n0 + delta_n;

        // Mean Anomaly
        double M = satEph.M_0 + n * tk;
        M = std::fmod(M + 2.0 * bdsPi, 2.0 * bdsPi);

        // Kepler's Equation for Eccentric Anomaly
        double E = M;
        for (int ii = 0; ii < 10; ++ii) {
            double E_old = E;
            E = M + satEph.e * std::sin(E);
            double dE = std::fmod(E - E_old, 2.0 * bdsPi);
            if (std::abs(dE) < 1.0e-12) break;
        }
        E = std::fmod(E + 2.0 * bdsPi, 2.0 * bdsPi);

        // True Anomaly
        double nu = std::atan2(std::sqrt(1.0 - satEph.e * satEph.e) * std::sin(E), std::cos(E) - satEph.e);

        // Argument of Latitude
        double phi = std::fmod(nu + satEph.omega, 2.0 * bdsPi);

        // Second Harmonic Perturbations
        double u = phi + satEph.C_uc * std::cos(2.0 * phi) + satEph.C_us * std::sin(2.0 * phi);
        double r = A * (1.0 - satEph.e * std::cos(E)) + satEph.C_rc * std::cos(2.0 * phi) + satEph.C_rs * std::sin(2.0 * phi);
        double i = satEph.i_0 + satEph.i_0Dot * tk + satEph.C_ic * std::cos(2.0 * phi) + satEph.C_is * std::sin(2.0 * phi);

        // Orbital Plane Position
        double xk1 = r * std::cos(u);
        double yk1 = r * std::sin(u);

        // Ascending Node Longitude
        double Omega = satEph.omega_0 + (satEph.omegaDot - OmegaE_dot) * tk - OmegaE_dot * satEph.t_oe;
        Omega = std::fmod(Omega + 2.0 * bdsPi, 2.0 * bdsPi);

        // ECEF Coordinates
        double xk = xk1 * std::cos(Omega) - yk1 * std::cos(i) * std::sin(Omega);
        double yk = xk1 * std::sin(Omega) + yk1 * std::cos(i) * std::cos(Omega);
        double zk = yk1 * std::sin(i);

        satPositions[0][satNr] = xk;
        satPositions[1][satNr] = yk;
        satPositions[2][satNr] = zk;

        // Relativistic Clock Correction
        double dtr = F * satEph.e * std::sqrt(A_0) * std::sin(E);
        satClkCorr[satNr] += dtr;

        // 3. Optional Satellite Velocity Computation
        double dE = n / (1.0 - satEph.e * std::cos(E));
        double dphi = std::sqrt(1.0 - satEph.e * satEph.e) * dE / (1.0 - satEph.e * std::cos(E));

        double du = dphi + 2.0 * dphi * (-satEph.C_uc * std::sin(2.0 * phi) + satEph.C_us * std::cos(2.0 * phi));
        double dr = A * satEph.e * dE * std::sin(E) + 2.0 * dphi * (-satEph.C_rc * std::sin(2.0 * phi) + satEph.C_rs * std::cos(2.0 * phi));
        double di = satEph.i_0Dot + 2.0 * dphi * (-satEph.C_ic * std::sin(2.0 * phi) + satEph.C_is * std::cos(2.0 * phi));

        double dxk1 = dr * std::cos(u) - r * du * std::sin(u);
        double dyk1 = dr * std::sin(u) + r * du * std::cos(u);

        double dOmega = satEph.omegaDot - OmegaE_dot;

        satVelocities[0][satNr] = -yk * dOmega - (dyk1 * std::cos(i) - zk * di) * std::sin(Omega) + dxk1 * std::cos(Omega);
        satVelocities[1][satNr] = xk * dOmega + (dyk1 * std::cos(i) - zk * di) * std::cos(Omega) + dxk1 * std::sin(Omega);
        satVelocities[2][satNr] = dyk1 * std::sin(i) + yk1 * di * std::cos(i);

        double dtrRat = F * satEph.e * std::sqrt(A_0) * std::cos(E) * dE;
        satClkDrift[satNr] = 2.0 * satEph.a_2 * dt + satEph.a_1 + dtrRat;
    }
}