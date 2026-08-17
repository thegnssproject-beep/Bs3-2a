#define _USE_MATH_DEFINES
#include <cmath>
#include "EphemerisDecoder.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

long long EphemerisDecoder::bin2dec(const std::string& bits)
{
    long long value = 0;
    for (char bit : bits) {
        if (bit == '1' || bit == '0') {
            value = (value << 1) | (bit - '0');
        }
    }
    return value;
}

long long EphemerisDecoder::twosComp2dec(const std::string& bits)
{
    if (bits.empty()) return 0;

    long long value = bin2dec(bits);
    size_t length = bits.length();

    if (bits[0] == '1') {
        value -= (1LL << length);
    }
    return value;
}

bool EphemerisDecoder::decodeSubframe(const std::string& navBitsBin, Ephemeris& eph)
{
    if (navBitsBin.length() < 288) {
        return false;
    }

    const double gpsPi = 3.1415926535898;

    // 1. Decode PRN (Bits 1:6 -> 0:5 0-indexed)
    int PRN = static_cast<int>(bin2dec(navBitsBin.substr(0, 6)));
    if (PRN < 1 || PRN > 63) {
        return false;
    }
    eph.PRN = PRN;

    // 2. Decode Message Type ID (Bits 7:12 -> 6:6)
    int mesType = static_cast<int>(bin2dec(navBitsBin.substr(6, 6)));

    // Decode SOW if not already set (Bits 13:30 -> 12:18)
    if (eph.SOW == 0.0) {
        eph.SOW = static_cast<double>(bin2dec(navBitsBin.substr(12, 18))) * 3.0;
    }

    switch (mesType)
    {
    case 10: // Message Type 10: Ephemeris Part 1
    {
        if (eph.idValid.size() < 8) eph.idValid.resize(8, 0);
        eph.idValid[0] = 10;

        eph.WN = static_cast<int>(bin2dec(navBitsBin.substr(30, 13)));
        eph.DIF = static_cast<int>(bin2dec(navBitsBin.substr(43, 1)));
        eph.SIF = static_cast<int>(bin2dec(navBitsBin.substr(44, 1)));
        eph.AIF = static_cast<int>(bin2dec(navBitsBin.substr(45, 1)));
        eph.t_oe = static_cast<double>(bin2dec(navBitsBin.substr(61, 11))) * 300.0;

        int satType = static_cast<int>(bin2dec(navBitsBin.substr(72, 2)));
        if (satType == 1)      eph.SatType = "GEO";
        else if (satType == 2) eph.SatType = "IGSO";
        else if (satType == 3) eph.SatType = "MEO";

        eph.deltaA = static_cast<double>(twosComp2dec(navBitsBin.substr(74, 26))) * std::pow(2.0, -9);
        eph.ADot = static_cast<double>(twosComp2dec(navBitsBin.substr(100, 25))) * std::pow(2.0, -21);
        eph.delta_n_0 = static_cast<double>(twosComp2dec(navBitsBin.substr(125, 17))) * std::pow(2.0, -44) * gpsPi;
        eph.delta_n_0Dot = static_cast<double>(twosComp2dec(navBitsBin.substr(142, 23))) * std::pow(2.0, -57) * gpsPi;
        eph.M_0 = static_cast<double>(twosComp2dec(navBitsBin.substr(165, 33))) * std::pow(2.0, -32) * gpsPi;
        eph.e = static_cast<double>(bin2dec(navBitsBin.substr(198, 33))) * std::pow(2.0, -34);
        eph.omega = static_cast<double>(twosComp2dec(navBitsBin.substr(231, 33))) * std::pow(2.0, -32) * gpsPi;
        break;
    }

    case 11: // Message Type 11: Ephemeris Part 2
    {
        if (eph.idValid.size() < 8) eph.idValid.resize(8, 0);
        eph.idValid[1] = 11;

        eph.HS = static_cast<int>(bin2dec(navBitsBin.substr(30, 2)));
        eph.DIF = static_cast<int>(bin2dec(navBitsBin.substr(32, 1)));
        eph.SIF = static_cast<int>(bin2dec(navBitsBin.substr(33, 1)));
        eph.AIF = static_cast<int>(bin2dec(navBitsBin.substr(35, 1)));

        eph.omega_0 = static_cast<double>(twosComp2dec(navBitsBin.substr(42, 33))) * std::pow(2.0, -32) * gpsPi;
        eph.i_0 = static_cast<double>(twosComp2dec(navBitsBin.substr(75, 33))) * std::pow(2.0, -32) * gpsPi;
        eph.omegaDot = static_cast<double>(twosComp2dec(navBitsBin.substr(108, 19))) * std::pow(2.0, -44) * gpsPi;
        eph.i_0Dot = static_cast<double>(twosComp2dec(navBitsBin.substr(127, 15))) * std::pow(2.0, -44) * gpsPi;

        eph.C_is = static_cast<double>(twosComp2dec(navBitsBin.substr(142, 16))) * std::pow(2.0, -30);
        eph.C_ic = static_cast<double>(twosComp2dec(navBitsBin.substr(158, 16))) * std::pow(2.0, -30);
        eph.C_rs = static_cast<double>(twosComp2dec(navBitsBin.substr(174, 24))) * std::pow(2.0, -8);
        eph.C_rc = static_cast<double>(twosComp2dec(navBitsBin.substr(198, 24))) * std::pow(2.0, -8);
        eph.C_us = static_cast<double>(twosComp2dec(navBitsBin.substr(222, 21))) * std::pow(2.0, -30);
        eph.C_uc = static_cast<double>(twosComp2dec(navBitsBin.substr(243, 21))) * std::pow(2.0, -30);
        break;
    }

    case 30: // Message Type 30: Clock, Ionosphere & Group Delay
    {
        if (eph.idValid.size() < 8) eph.idValid.resize(8, 0);
        eph.idValid[2] = 30;

        eph.t_oc = static_cast<double>(bin2dec(navBitsBin.substr(42, 11))) * 300.0;
        eph.a_0 = static_cast<double>(twosComp2dec(navBitsBin.substr(53, 25))) * std::pow(2.0, -34);
        eph.a_1 = static_cast<double>(twosComp2dec(navBitsBin.substr(78, 22))) * std::pow(2.0, -50);
        eph.a_2 = static_cast<double>(twosComp2dec(navBitsBin.substr(100, 11))) * std::pow(2.0, -66);

        eph.IODC_MSB2 = static_cast<int>(bin2dec(navBitsBin.substr(111, 2)));
        eph.IODC_LSB8 = static_cast<int>(bin2dec(navBitsBin.substr(113, 8)));

        eph.alpha1 = static_cast<double>(bin2dec(navBitsBin.substr(145, 10))) * std::pow(2.0, -3);
        eph.alpha2 = static_cast<double>(twosComp2dec(navBitsBin.substr(155, 8))) * std::pow(2.0, -3);
        eph.alpha3 = static_cast<double>(bin2dec(navBitsBin.substr(163, 8))) * std::pow(2.0, -3);
        eph.alpha4 = static_cast<double>(bin2dec(navBitsBin.substr(171, 8))) * std::pow(2.0, -3);
        eph.alpha5 = static_cast<double>(bin2dec(navBitsBin.substr(179, 8))) * std::pow(2.0, -3);
        eph.alpha6 = static_cast<double>(twosComp2dec(navBitsBin.substr(187, 8))) * std::pow(2.0, -3);
        eph.alpha7 = static_cast<double>(twosComp2dec(navBitsBin.substr(195, 8))) * std::pow(2.0, -3);
        eph.alpha8 = static_cast<double>(twosComp2dec(navBitsBin.substr(203, 8))) * std::pow(2.0, -3);
        eph.alpha9 = static_cast<double>(twosComp2dec(navBitsBin.substr(211, 8))) * std::pow(2.0, -3);
        break;
    }

    case 31: // Message Type 31: Clock & Reduced Almanac
    case 32: // Message Type 32: Clock & EOP
    case 33: // Message Type 33: Clock & UTC
    case 34: // Message Type 34: Clock & Differential Corrections
    {
        int validIdx = mesType - 31 + 3;
        if (eph.idValid.size() < 8) eph.idValid.resize(8, 0);
        eph.idValid[validIdx] = mesType;

        size_t tocOffset = (mesType == 34) ? 64 : 42;
        size_t a0Offset = (mesType == 34) ? 75 : 53;
        size_t a1Offset = (mesType == 34) ? 100 : 78;
        size_t a2Offset = (mesType == 34) ? 122 : 100;

        eph.t_oc = static_cast<double>(bin2dec(navBitsBin.substr(tocOffset, 11))) * 300.0;
        eph.a_0 = static_cast<double>(twosComp2dec(navBitsBin.substr(a0Offset, 25))) * std::pow(2.0, -34);
        eph.a_1 = static_cast<double>(twosComp2dec(navBitsBin.substr(a1Offset, 22))) * std::pow(2.0, -50);
        eph.a_2 = static_cast<double>(twosComp2dec(navBitsBin.substr(a2Offset, 11))) * std::pow(2.0, -66);
        break;
    }

    default:
        if (eph.idValid.size() < 8) eph.idValid.resize(8, 0);
        eph.idValid[7] = mesType;
        break;
    }

    return true;
}