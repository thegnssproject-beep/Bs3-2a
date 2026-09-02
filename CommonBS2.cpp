#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

#include "CommonBS2.h"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <numeric>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace CommonBS2 {

    std::vector<double> e_r_corr(double traveltime, const std::vector<double>& X_sat)
    {
        double Omegae_dot = 7.292115147e-5;
        double omegatau = Omegae_dot * traveltime;

        double cosA = std::cos(omegatau);
        double sinA = std::sin(omegatau);

        std::vector<double> X_sat_rot(3);
        X_sat_rot[0] = cosA * X_sat[0] + sinA * X_sat[1];
        X_sat_rot[1] = -sinA * X_sat[0] + cosA * X_sat[1];
        X_sat_rot[2] = X_sat[2];

        return X_sat_rot;
    }

    void cart2geo(double X, double Y, double Z, int i, double& phi, double& lambda, double& h)
    {
        double a[] = { 0, 6378388, 6378160, 6378135, 6378137, 6378137 };
        double f[] = { 0, 1 / 297.0, 1 / 298.247, 1 / 298.26, 1 / 298.257222101, 1 / 298.257223563 };

        if (i < 1 || i > 5) i = 5;

        lambda = std::atan2(Y, X);
        double ex2 = (2.0 - f[i]) * f[i] / std::pow(1.0 - f[i], 2.0);
        double c = a[i] * std::sqrt(1.0 + ex2);
        phi = std::atan(Z / (std::sqrt(X * X + Y * Y) * (1.0 - (2.0 - f[i])) * f[i]));

        h = 0.1;
        double oldh = 0.0;
        int iterations = 0;

        while (std::abs(h - oldh) > 1.e-12) {
            oldh = h;
            double N = c / std::sqrt(1.0 + ex2 * std::pow(std::cos(phi), 2.0));
            phi = std::atan(Z / (std::sqrt(X * X + Y * Y) * (1.0 - (2.0 - f[i]) * f[i] * N / (N + h))));
            h = std::sqrt(X * X + Y * Y) / std::cos(phi) - N;

            if (++iterations > 100) break;
        }

        phi = phi * 180.0 / M_PI;
        lambda = lambda * 180.0 / M_PI;
    }

    void topocent(const std::vector<double>& X, const std::vector<double>& dx, double& az, double& el, double& dist) {
        double dtr = M_PI / 180.0;

        double phi = 0.0, lambda = 0.0, h = 0.0;
        cart2geo(X[0], X[1], X[2], 4, phi, lambda, h);

        double cl = std::cos(lambda * dtr);
        double sl = std::sin(lambda * dtr);
        double cb = std::cos(phi * dtr);
        double sb = std::sin(phi * dtr);

        double E = -sl * dx[0] + cl * dx[1];
        double N = -sb * cl * dx[0] - sb * sl * dx[1] + cb * dx[2];
        double U = cb * cl * dx[0] + cb * sl * dx[1] + sb * dx[2];

        double hor_dis = std::sqrt(E * E + N * N);

        if (hor_dis < 1.e-20) {
            az = 0.0;
            el = 90.0;
        }
        else {
            az = std::atan2(E, N) / dtr;
            el = std::atan2(U, hor_dis) / dtr;
        }

        if (az < 0.0) az += 360.0;
        dist = std::sqrt(dx[0] * dx[0] + dx[1] * dx[1] + dx[2] * dx[2]);
    }

    double tropo(double sinel, double hsta, double p, double tkel, double hum, double hp, double htkel, double hhum)
    {
        double a_e = 6378.137;
        double b0 = 7.839257e-5;
        double tlapse = -6.5;
        double tkhum = tkel + tlapse * (hhum - htkel);
        double atkel = 7.5 * (tkhum - 273.15) / (237.3 + tkhum - 273.15);
        double e0 = 0.0611 * hum * std::pow(10.0, atkel);
        double tksea = tkel - tlapse * htkel;
        double em = -978.77 / (2.8704e6 * tlapse * 1.0e-5);
        double tkelh = tksea + tlapse * hhum;
        double e0sea = e0 * std::pow(tksea / tkelh, 4.0 * em);
        double tkelp = tksea + tlapse * hp;
        double psea = p * std::pow(tksea / tkelp, em);

        if (sinel < 0.0) sinel = 0.0;

        double tropo_val = 0.0;
        bool done = false;
        double refsea = 77.624e-6 / tksea;
        double htop = 1.1385e-5 / refsea;
        refsea = refsea * psea;
        double ref = refsea * std::pow((htop - hsta) / htop, 4.0);

        while (true) {
            double rtop = std::pow(a_e + htop, 2.0) - std::pow(a_e + hsta, 2.0) * (1.0 - sinel * sinel);
            if (rtop < 0.0) rtop = 0.0;

            rtop = std::sqrt(rtop) - (a_e + hsta) * sinel;
            double a = -sinel / (htop - hsta);
            double b = -b0 * (1.0 - sinel * sinel) / (htop - hsta);

            std::vector<double> rn(8);
            for (int i = 0; i < 8; ++i) {
                rn[i] = std::pow(rtop, i + 2);
            }

            std::vector<double> alpha = {
                2 * a, 2 * a * a + 4 * b / 3.0, a * (a * a + 3 * b),
                std::pow(a,4) / 5.0 + 2.4 * a * a * b + 1.2 * b * b, 2 * a * b * (a * a + 3 * b) / 3.0,
                b * b * (6 * a * a + 4 * b) * 1.428571e-1, 0, 0
            };

            if (b * b > 1.0e-35) {
                alpha[6] = a * std::pow(b, 3) / 2.0;
                alpha[7] = std::pow(b, 4) / 9.0;
            }

            double dr = rtop;
            for (int i = 0; i < 8; ++i) dr += alpha[i] * rn[i];

            tropo_val += dr * ref * 1000.0;

            if (done) return tropo_val;

            done = true;
            refsea = (371900.0e-6 / tksea - 12.92e-6) / tksea;
            htop = 1.1385e-5 * (1255.0 / tksea + 0.05) / refsea;
            ref = refsea * e0sea * std::pow((htop - hsta) / htop, 4.0);
        }
    }

    void calculatePseudoranges(
        const std::vector<ChannelTrackResult>& trackResults,
        const std::vector<int>& subFrameStart,
        const std::vector<double>& TOW,
        double currMeasSample,
        double& localTime,
        const std::vector<int>& channelList,
        const Settings& settings,
        std::vector<double>& pseudoranges,
        std::vector<double>& transmitTime)
    {
        static std::vector<size_t> searchIndex;

        if (searchIndex.empty() || localTime >= 1e20) {
            searchIndex.assign(settings.numberOfChannels, 0);
        }

        transmitTime.assign(settings.numberOfChannels, 1e30);
        pseudoranges.assign(settings.numberOfChannels, 0.0);

        for (int channelNr : channelList) {
            size_t index = searchIndex[channelNr];

            while (index < trackResults[channelNr].absoluteSample.size()) {
                if (trackResults[channelNr].absoluteSample[index] > currMeasSample) {
                    break;
                }
                index++;
            }
            searchIndex[channelNr] = index;

            if (index > 0) index--;

            double codePhaseStep = trackResults[channelNr].codeFreq[index] / settings.samplingFreq;

            double codePhase = trackResults[channelNr].remCodePhase[index] +
                codePhaseStep * (currMeasSample - trackResults[channelNr].absoluteSample[index]);

            transmitTime[channelNr] = (codePhase / settings.codeLength + index - subFrameStart[channelNr]) *
                settings.codeLength / settings.codeFreqBasis + TOW[channelNr];
        }

        if (localTime >= 1e20) {
            double maxTime = -1e30;
            for (int channelNr : channelList) {
                if (transmitTime[channelNr] < 1e20 && transmitTime[channelNr] > maxTime) {
                    maxTime = transmitTime[channelNr];
                }
            }
            localTime = maxTime + settings.startOffset / 1000.0;
        }

        for (int channelNr : channelList) {
            if (transmitTime[channelNr] < 1e20) {
                pseudoranges[channelNr] = (localTime - transmitTime[channelNr]) * settings.c;
            }
        }
    }

    bool invert4x4(const double m[4][4], double inv[4][4])
    {
        double a[4][8] = { {0} };

        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                a[i][j] = m[i][j];
            }
            a[i][i + 4] = 1.0;
        }

        for (int i = 0; i < 4; ++i) {
            int pivot = i;
            for (int j = i + 1; j < 4; ++j) {
                if (std::abs(a[j][i]) > std::abs(a[pivot][i])) {
                    pivot = j;
                }
            }

            if (std::abs(a[pivot][i]) < 1e-12) return false;

            for (int k = 0; k < 8; ++k) {
                std::swap(a[i][k], a[pivot][k]);
            }

            double div = a[i][i];
            for (int k = 0; k < 8; ++k) {
                a[i][k] /= div;
            }

            for (int j = 0; j < 4; ++j) {
                if (j != i) {
                    double factor = a[j][i];
                    for (int k = 0; k < 8; ++k) {
                        a[j][k] -= factor * a[i][k];
                    }
                }
            }
        }

        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                inv[i][j] = a[i][j + 4];
            }
        }

        return true;
    }

    bool leastSquarePos(
        const std::vector<std::vector<double>>& satpos,
        const std::vector<double>& obs,
        const Settings& settings,
        std::vector<double>& pos,
        std::vector<double>& el,
        std::vector<double>& az,
        std::vector<double>& dop)
    {
        int nmbOfIterations = 10;
        double dtr = M_PI / 180.0;

        pos.assign(4, 0.0);
        size_t nmbOfSatellites = satpos.size();

        az.assign(nmbOfSatellites, 0.0);
        el.assign(nmbOfSatellites, 0.0);
        dop.assign(5, 0.0);

        if (nmbOfSatellites < 4) return false;

        double A[32][4] = { {0} };
        double omc[32] = { 0 };

        for (int iter = 0; iter < nmbOfIterations; ++iter) {
            double AtA[4][4] = { {0} };
            double Atb[4] = { 0 };

            for (size_t i = 0; i < nmbOfSatellites; ++i) {
                std::vector<double> Rot_X(3);
                double trop = 2.0;

                if (iter == 0) {
                    Rot_X = satpos[i];
                }
                else {
                    double dx = satpos[i][0] - pos[0];
                    double dy = satpos[i][1] - pos[1];
                    double dz = satpos[i][2] - pos[2];
                    double rho2 = dx * dx + dy * dy + dz * dz;
                    double traveltime = std::sqrt(rho2) / settings.c;

                    Rot_X = e_r_corr(traveltime, satpos[i]);

                    std::vector<double> userPos = { pos[0], pos[1], pos[2] };
                    std::vector<double> diff = { Rot_X[0] - pos[0], Rot_X[1] - pos[1], Rot_X[2] - pos[2] };

                    double dist = 0.0;
                    topocent(userPos, diff, az[i], el[i], dist);

                    if (settings.useTropCorr == 1) {
                        trop = tropo(std::sin(el[i] * dtr), 0.0, 1013.0, 293.0, 50.0, 0.0, 0.0, 0.0);
                    }
                    else {
                        trop = 0.0;
                    }
                }

                double dx = Rot_X[0] - pos[0];
                double dy = Rot_X[1] - pos[1];
                double dz = Rot_X[2] - pos[2];
                double normRx = std::sqrt(dx * dx + dy * dy + dz * dz);

                omc[i] = obs[i] - normRx - pos[3] - trop;

                A[i][0] = -dx / normRx;
                A[i][1] = -dy / normRx;
                A[i][2] = -dz / normRx;
                A[i][3] = 1.0;

                for (int r = 0; r < 4; ++r) {
                    for (int c = 0; c < 4; ++c) {
                        AtA[r][c] += A[i][r] * A[i][c];
                    }
                    Atb[r] += A[i][r] * omc[i];
                }
            }

            double invAtA[4][4];
            if (!invert4x4(AtA, invAtA)) {
                return false;
            }

            double x_upd[4] = { 0 };
            for (int r = 0; r < 4; ++r) {
                for (int c = 0; c < 4; ++c) {
                    x_upd[r] += invAtA[r][c] * Atb[c];
                }
                pos[r] += x_upd[r];
            }

            if (iter == nmbOfIterations - 1) {
                dop[0] = std::sqrt(invAtA[0][0] + invAtA[1][1] + invAtA[2][2] + invAtA[3][3]);
                dop[1] = std::sqrt(invAtA[0][0] + invAtA[1][1] + invAtA[2][2]);
                dop[2] = std::sqrt(invAtA[0][0] + invAtA[1][1]);
                dop[3] = std::sqrt(invAtA[2][2]);
                dop[4] = std::sqrt(invAtA[3][3]);
            }
        }
        return true;
    }

    double CNoVSM(const std::vector<double>& I, const std::vector<double>& Q, double T)
    {
        if (I.empty() || I.size() != Q.size()) return 0.0;

        double Zm = 0.0;
        size_t K = I.size();
        std::vector<double> Z(K);

        for (size_t k = 0; k < K; ++k) {
            Z[k] = I[k] * I[k] + Q[k] * Q[k];
            Zm += Z[k];
        }
        Zm /= static_cast<double>(K);

        double Zv = 0.0;
        for (size_t k = 0; k < K; ++k) {
            Zv += (Z[k] - Zm) * (Z[k] - Zm);
        }
        Zv /= static_cast<double>(K - 1);

        double Pav = std::sqrt(std::max(0.0, Zm * Zm - Zv));
        double Nv = 0.5 * (Zm - Pav);

        if (Nv <= 1e-12) return 50.0;
        return 10.0 * std::log10(std::abs((1.0 / T) * Pav / (2.0 * Nv)));
    }

    int navPartyChk(std::vector<int> ndat)
    {
        if (ndat.size() < 32) return 0;

        if (ndat[1] != 1) {
            for (int i = 2; i <= 25; ++i) {
                ndat[i] = -1 * ndat[i];
            }
        }

        std::vector<int> parity(6);
        parity[0] = ndat[0] * ndat[2] * ndat[3] * ndat[4] * ndat[6] * ndat[7] * ndat[11] * ndat[12] * ndat[13] * ndat[14] * ndat[15] * ndat[18] * ndat[19] * ndat[21] * ndat[24];
        parity[1] = ndat[1] * ndat[3] * ndat[4] * ndat[5] * ndat[7] * ndat[8] * ndat[12] * ndat[13] * ndat[14] * ndat[15] * ndat[16] * ndat[19] * ndat[20] * ndat[22] * ndat[25];
        parity[2] = ndat[0] * ndat[2] * ndat[4] * ndat[5] * ndat[6] * ndat[8] * ndat[9] * ndat[13] * ndat[14] * ndat[15] * ndat[16] * ndat[17] * ndat[20] * ndat[21] * ndat[23];
        parity[3] = ndat[1] * ndat[3] * ndat[5] * ndat[6] * ndat[7] * ndat[9] * ndat[10] * ndat[14] * ndat[15] * ndat[16] * ndat[17] * ndat[18] * ndat[21] * ndat[22] * ndat[24];
        parity[4] = ndat[1] * ndat[2] * ndat[4] * ndat[6] * ndat[7] * ndat[8] * ndat[10] * ndat[11] * ndat[15] * ndat[16] * ndat[17] * ndat[18] * ndat[19] * ndat[22] * ndat[23] * ndat[25];
        parity[5] = ndat[0] * ndat[4] * ndat[6] * ndat[7] * ndat[9] * ndat[10] * ndat[11] * ndat[12] * ndat[14] * ndat[16] * ndat[20] * ndat[23] * ndat[24] * ndat[25];

        int matchCount = 0;
        for (int i = 0; i < 6; ++i) {
            if (parity[i] == ndat[26 + i]) matchCount++;
        }

        if (matchCount == 6) {
            return -1 * ndat[1];
        }
        return 0;
    }

    int64_t twosComp2dec(const std::string& binaryNumber)
    {
        if (binaryNumber.empty()) return 0;
        bool isNeg = (binaryNumber[0] == '1');
        int64_t val = 0;
        for (char c : binaryNumber) {
            val = (val << 1) | (c == '1' ? 1 : 0);
        }
        if (isNeg) {
            val -= (1LL << binaryNumber.size());
        }
        return val;
    }

    void cart2utm(double X, double Y, double Z, int zone, double& E, double& N, double& U)
    {
        double a = 6378388;
        double f = 1.0 / 297.0;
        double ex2 = (2.0 - f) * f / std::pow(1.0 - f, 2.0);
        double c = a * std::sqrt(1.0 + ex2);

        double vec[3] = { X, Y, Z - 4.5 };
        double alpha = 0.756e-6;
        double scale = 0.9999988;

        double v[3];
        v[0] = scale * (vec[0] - alpha * vec[1]) + 89.5;
        v[1] = scale * (alpha * vec[0] + vec[1]) + 93.8;
        v[2] = scale * vec[2] + 127.6;

        double L = std::atan2(v[1], v[0]);
        double N1 = 6395000.0;
        double normXY = std::sqrt(v[0] * v[0] + v[1] * v[1]);
        double B = std::atan2(v[2] / (std::pow(1.0 - f, 2.0) * N1), normXY / N1);

        U = 0.1;
        double oldU = 0.0;
        while (std::abs(U - oldU) > 1.e-4) {
            oldU = U;
            N1 = c / std::sqrt(1.0 + ex2 * std::pow(std::cos(B), 2.0));
            B = std::atan2(v[2] / (std::pow(1.0 - f, 2.0) * N1 + U), normXY / (N1 + U));
            U = normXY / std::cos(B) - N1;
        }

        double m0 = 0.0004;
        double n = f / (2.0 - f);
        double m = n * n * (1.0 / 4.0 + n * n / 64.0);
        double w = (a * (-n - m0 + m * (1.0 - m0))) / (1.0 + n);
        double Q_n = a + w;

        double E0 = 500000.0;
        double L0 = (zone - 30) * 6.0 - 3.0;

        std::vector<double> bg = { -3.37077907e-3, 4.73444769e-6, -8.29914570e-9, 1.58785330e-11 };
        std::vector<double> gtu = { 8.41275991e-4, 7.67306686e-7, 1.21291230e-9, 2.48508228e-12 };

        bool neg_geo = (B < 0);
        double Bg_r = std::abs(B);
        Bg_r += clsin(bg, 4, 2.0 * Bg_r);

        L0 = L0 * M_PI / 180.0;
        double Lg_r = L - L0;

        double cos_BN = std::cos(Bg_r);
        double Np = std::atan2(std::sin(Bg_r), std::cos(Lg_r) * cos_BN);
        double Ep = std::atanh(std::sin(Lg_r) * cos_BN);

        Np *= 2.0;
        Ep *= 2.0;
        double dN, dE;
        clksin(gtu, 4, Np, Ep, dN, dE);

        Np = Np / 2.0 + dN;
        Ep = Ep / 2.0 + dE;

        N = Q_n * Np;
        E = Q_n * Ep + E0;

        if (neg_geo) {
            N = -N + 20000000.0;
        }
    }

    void calcLoopCoefCarr(double LBW, double intTime, double& pf3, double& pf2, double& pf1)
    {
        double a3 = 2.0;
        double b3 = 2.0;
        double Wn = 1.2 * LBW;

        pf3 = std::pow(Wn, 3) * std::pow(intTime, 2);
        pf2 = a3 * std::pow(Wn, 2) * intTime;
        pf1 = b3 * Wn;
    }

    int findUtmZone(double latitude, double longitude)
    {
        if (longitude > 180.0 || longitude < -180.0 || latitude > 84.0 || latitude < -80.0) {
            return -1;
        }

        int utmZone = static_cast<int>((180.0 + longitude) / 6.0) + 1;

        if (latitude > 72.0) {
            if (longitude >= 0.0 && longitude < 9.0) utmZone = 31;
            else if (longitude >= 9.0 && longitude < 21.0) utmZone = 33;
            else if (longitude >= 21.0 && longitude < 33.0) utmZone = 35;
            else if (longitude >= 33.0 && longitude < 42.0) utmZone = 37;
        }
        else if (latitude >= 56.0 && latitude < 64.0) {
            if (longitude >= 3.0 && longitude < 12.0) utmZone = 32;
        }

        return utmZone;
    }

    double roundn(double x, int n)
    {
        double factor = std::pow(10.0, std::floor(-n));
        return std::round(x * factor) / factor;
    }

    double deg2dms(double deg)
    {
        bool neg_arg = false;
        if (deg < 0.0) {
            deg = -deg;
            neg_arg = true;
        }

        double int_deg = std::floor(deg);
        double decimal = deg - int_deg;
        double min_part = decimal * 60.0;
        double m = std::floor(min_part);
        double sec_part = min_part - m;
        double s = sec_part * 60.0;

        if (s >= 60.0) {
            m += 1.0;
            s = 0.0;
        }
        if (m >= 60.0) {
            int_deg += 1.0;
            m = 0.0;
        }

        double dmsOutput = int_deg * 100.0 + m + s / 100.0;
        if (neg_arg) {
            dmsOutput = -dmsOutput;
        }

        return dmsOutput;
    }

    void dms2mat(double dms, int n, double& d, double& m, double& s)
    {
        if (n == 2) n = 1;

        double signvec = (dms < 0.0) ? -1.0 : 1.0;
        dms = std::abs(dms);

        d = std::floor(dms / 100.0);
        m = std::floor(dms) - std::abs(100.0 * d);
        s = roundn(100.0 * std::fmod(dms, 1.0), n);

        if (s >= 60.0) { m += 1.0; s -= 60.0; }
        if (m >= 60.0) { d += 1.0; m -= 60.0; }

        double dsign = (d != 0) ? signvec : 0;
        double msign = (d == 0 && m != 0) ? signvec : 0;
        double ssign = (d == 0 && m == 0 && s != 0) ? signvec : 0;

        d = ((dsign == 0) ? 1.0 : dsign) * d;
        m = ((msign == 0) ? 1.0 : msign) * m;
        s = ((ssign == 0) ? 1.0 : ssign) * s;
    }

    double mat2dms(double d, double m, double s, int n)
    {
        if (n == 2) n = 1;

        bool negvec = (d < 0.0) || (m < 0.0) || (s < 0.0);
        double signvec = negvec ? -1.0 : 1.0;

        d = std::abs(d);
        m = std::abs(m);
        s = std::abs(s);

        s = roundn(s, n);

        if (s >= 60.0) { m += 1.0; s = 0.0; }
        if (m >= 60.0) { d += 1.0; m -= 60.0; }

        return signvec * (100.0 * d + m + s / 100.0);
    }

    double clsin(const std::vector<double>& ar, int degree, double argument) {
        double cos_arg = 2.0 * std::cos(argument);
        double hr1 = 0.0;
        double hr = 0.0;

        for (int t = degree - 1; t >= 0; --t) {
            double hr2 = hr1;
            hr1 = hr;
            hr = ar[t] + cos_arg * hr1 - hr2;
        }

        return hr * std::sin(argument);
    }

    void clksin(const std::vector<double>& ar, int degree, double arg_real, double arg_imag, double& re, double& im) {
        double sin_arg_r = std::sin(arg_real);
        double cos_arg_r = std::cos(arg_real);
        double sinh_arg_i = std::sinh(arg_imag);
        double cosh_arg_i = std::cosh(arg_imag);

        double r = 2.0 * cos_arg_r * cosh_arg_i;
        double i_val = -2.0 * sin_arg_r * sinh_arg_i;

        double hr1 = 0.0, hr = 0.0, hi1 = 0.0, hi = 0.0;

        for (int t = degree - 1; t >= 0; --t) {
            double hr2 = hr1;
            hr1 = hr;
            double hi2 = hi1;
            hi1 = hi;

            double z = ar[t] + r * hr1 - i_val * hi - hr2;
            hi = i_val * hr1 + r * hi1 - hi2;
            hr = z;
        }

        r = sin_arg_r * cosh_arg_i;
        i_val = cos_arg_r * sinh_arg_i;

        re = r * hr - i_val * hi;
        im = r * hi + i_val * hr;
    }
}