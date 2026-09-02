#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "Ephemeris.h"

class EphemerisDecoder
{
public:
    static bool decodeSubframe(const std::string& navBitsBin, Ephemeris& eph);
    static long long bin2dec(const std::string& bits);
    static long long twosComp2dec(const std::string& bits);

    // CRC-24Q Parity Check & Frame Sync Functions
    static bool checkCRC24Q(const std::string& bitStr288);
    static bool findPreambleAndDecode(const std::vector<double>& prompt_I, Ephemeris& eph, int& subFrameStartIdx, double& detectedTOW);
    static bool findAllEphemerisFrames(const std::vector<double>& prompt_I, Ephemeris& eph, int& firstFrameStartIdx, double& firstTOW);
};