#pragma once

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

#include <vector>
#include <string>
#include <cstdint>
#include <cmath>

#include "Settings.h"
#include "Tracking.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace CommonBS2 {
    void calculatePseudoranges(
        const std::vector<ChannelTrackResult>& trackResults,
        const std::vector<int>& subFrameStart,
        const std::vector<double>& TOW,
        double currMeasSample,
        double& localTime,
        const std::vector<int>& channelList,
        const Settings& settings,
        std::vector<double>& pseudoranges,
        std::vector<double>& transmitTime
    );

    bool invert4x4(const double m[4][4], double inv[4][4]);

    std::vector<double> e_r_corr(double traveltime, const std::vector<double>& X_sat);
    void cart2geo(double X, double Y, double Z, int i, double& phi, double& lambda, double& h);
    void topocent(const std::vector<double>& X, const std::vector<double>& dx, double& az, double& el, double& dist);
    double tropo(double sinel, double hsta, double p, double tkel, double hum, double hp, double htkel, double hhum);
    double clsin(const std::vector<double>& ar, int degree, double argument);
    void clksin(const std::vector<double>& ar, int degree, double arg_real, double arg_imag, double& re, double& im);

    bool leastSquarePos(
        const std::vector<std::vector<double>>& satpos,
        const std::vector<double>& obs,
        const Settings& settings,
        std::vector<double>& pos,
        std::vector<double>& el,
        std::vector<double>& az,
        std::vector<double>& dop
    );

    double CNoVSM(const std::vector<double>& I, const std::vector<double>& Q, double T);
    int navPartyChk(std::vector<int> ndat);
    int64_t twosComp2dec(const std::string& binaryNumber);
    void cart2utm(double X, double Y, double Z, int zone, double& E, double& N, double& U);
    void calcLoopCoefCarr(double LBW, double intTime, double& pf3, double& pf2, double& pf1);
    int findUtmZone(double latitude, double longitude);
    double roundn(double x, int n);
    double deg2dms(double deg);
    void dms2mat(double dms, int n, double& d, double& m, double& s);
    double mat2dms(double d, double m, double s, int n);
}