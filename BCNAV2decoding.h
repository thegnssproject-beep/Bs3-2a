#pragma once

#include <vector>
#include <cmath>
#include <iostream>
#include <algorithm>
#include <limits>

// Ephemeris structure initialized for B-CNAV2
struct EphStructure
{
    int PRN = 0;
    std::vector<int> idValid = std::vector<int>(8, 0); // Message types parsed (10, 11, 30-34)
    double SOW = 0.0; // Seconds of Week
    double toe = 0.0;
    double toc = 0.0;
    double a = 0.0;
    double e = 0.0;
    double i0 = 0.0;
    double Omega0 = 0.0;
    double omega = 0.0;
    double M0 = 0.0;
    double deltan = 0.0;
    double Omegadot = 0.0;
    double idot = 0.0;
    double crc = 0.0, crs = 0.0, cuc = 0.0, cus = 0.0, cic = 0.0, cis = 0.0;
    double af0 = 0.0, af1 = 0.0, af2 = 0.0;
};

class BCNAV2Decoder
{
public:
    static bool checkCRC24Q(const std::vector<int>& bits);
    static EphStructure decode(const std::vector<double>& I_P_InputBits, double& firstSubFrame, double& TOW);

private:
    static EphStructure parseEphemerisMessage(const std::vector<int>& bits, EphStructure eph);
};