#pragma once

#include <string>
#include <vector>

struct Ephemeris {
    int PRN = 0;
    double SOW = 0.0;
    std::vector<int> idValid = std::vector<int>(8, 0);

    // Message 10 Parameters
    int WN = 0;
    int DIF = 0;
    int SIF = 0;
    int AIF = 0;
    double t_oe = 0.0;
    std::string SatType = "MEO";
    double deltaA = 0.0;
    double ADot = 0.0;
    double delta_n_0 = 0.0;
    double delta_n_0Dot = 0.0;
    double M_0 = 0.0;
    double e = 0.0;
    double omega = 0.0;

    // Message 11 Parameters
    int HS = 0;
    double omega_0 = 0.0;
    double i_0 = 0.0;
    double omegaDot = 0.0;
    double i_0Dot = 0.0;
    double C_is = 0.0;
    double C_ic = 0.0;
    double C_rs = 0.0;
    double C_rc = 0.0;
    double C_us = 0.0;
    double C_uc = 0.0;

    // Message 30-34 (Clock & Ionosphere)
    double t_oc = 0.0;
    double a_0 = 0.0;
    double a_1 = 0.0;
    double a_2 = 0.0;
    double T_GDB1Cp = 0.0;
    int IODC_MSB2 = 0;
    int IODC_LSB8 = 0;

    // Ionosphere parameters
    double alpha1 = 0.0, alpha2 = 0.0, alpha3 = 0.0;
    double alpha4 = 0.0, alpha5 = 0.0, alpha6 = 0.0;
    double alpha7 = 0.0, alpha8 = 0.0, alpha9 = 0.0;
};