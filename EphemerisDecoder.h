#pragma once

#include <string>
#include <vector>
#include <cmath>
#include "Ephemeris.h"

class EphemerisDecoder
{
public:
    static bool decodeSubframe(const std::string& navBitsBin, Ephemeris& eph);
    static long long bin2dec(const std::string& bits);
    static long long twosComp2dec(const std::string& bits);
};