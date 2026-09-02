#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

#include "EphemerisDecoder.h"
#include <cmath>
#include <algorithm>
#include <set>

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

bool EphemerisDecoder::checkCRC24Q(const std::string& bitStr288)
{
    if (bitStr288.length() < 288) return false;

    constexpr uint32_t POLY = 0x1864CFB;
    uint32_t crc = 0x000000;

    for (size_t i = 0; i < 288; ++i) {
        uint32_t bit = (bitStr288[i] == '1') ? 1 : 0;
        crc = (crc << 1) | bit;
        if (crc & 0x1000000) {
            crc ^= POLY;
        }
    }
    return (crc & 0xFFFFFF) == 0;
}

// B-CNAV2 is broadcast at a symbol rate of 200 sps, i.e. each data symbol spans
// 5 ms (5 integrate-and-dump samples at 1 kHz). The tracker produces one I_P
// per ms, so we must first recover the 5-ms symbol boundaries (bit sync) and
// then majority-vote each 5-sample group into one data symbol. Without this,
// every frame search misses the preamble and the navigation message can never
// be decoded (fixes stay at 0, az/el stay at zenith in the sky plot).
//
// Returns the recovered 200-sps symbol bit-string in `outBits` and sets
// `firstSymbolSampleIdx` to the original 1-ms sample index of the first symbol
// (used to map symbol positions back to absolute tracking samples).
static bool extractB2aSymbolBits(const std::vector<double>& prompt_I,
    std::string& outBits, int& firstSymbolSampleIdx)
{
    const int SAMPLES_PER_SYMBOL = 5;
    const size_t N = prompt_I.size();
    if (N < 24 * SAMPLES_PER_SYMBOL) return false;

    // Pick the sync phase (0..4) whose 5-sample grouping maximises the summed
    // symbol magnitude (i.e. where the five samples of each symbol agree, so
    // majority voting is most reliable).
    double bestScore = -1.0;
    int bestPhase = 0;
    for (int phase = 0; phase < SAMPLES_PER_SYMBOL; ++phase) {
        double score = 0.0;
        size_t nGroups = 0;
        for (size_t i = phase; i + SAMPLES_PER_SYMBOL <= N; i += SAMPLES_PER_SYMBOL) {
            double s = 0.0;
            for (int k = 0; k < SAMPLES_PER_SYMBOL; ++k) s += prompt_I[i + k];
            score += std::abs(s);
            nGroups++;
        }
        if (nGroups > 0) score /= static_cast<double>(nGroups);
        if (score > bestScore) { bestScore = score; bestPhase = phase; }
    }

    outBits.clear();
    outBits.reserve(N / SAMPLES_PER_SYMBOL);
    for (size_t i = static_cast<size_t>(bestPhase); i + SAMPLES_PER_SYMBOL <= N; i += SAMPLES_PER_SYMBOL) {
        double s = 0.0;
        for (int k = 0; k < SAMPLES_PER_SYMBOL; ++k) s += prompt_I[i + k];
        outBits += (s >= 0.0) ? '1' : '0';
    }

    firstSymbolSampleIdx = bestPhase;
    return outBits.size() >= 24;
}

bool EphemerisDecoder::findPreambleAndDecode(const std::vector<double>& prompt_I, Ephemeris& eph, int& subFrameStartIdx, double& detectedTOW)
{
    // B-CNAV2 preamble: 24 symbols = 0xE24DE8 (MSB first). The 288-bit message
    // (PRN/MesType/SOW/data/CRC) follows the 24-symbol preamble.
    const std::string PREAMBLE_NORM = "111000100100110111101000";
    const std::string PREAMBLE_INV = "000111011011001000010111";

    std::string bits;
    int firstSymbolSampleIdx = 0;
    if (!extractB2aSymbolBits(prompt_I, bits, firstSymbolSampleIdx)) return false;
    if (bits.size() < 24 + 288) return false;

    // Search across the whole recovered 200-sps symbol stream.
    for (size_t i = 0; i + 24 + 288 <= bits.size(); ++i) {
        std::string pre = bits.substr(i, 24);
        bool isInverted = false;
        if (pre == PREAMBLE_NORM) isInverted = false;
        else if (pre == PREAMBLE_INV) isInverted = true;
        else continue;

        // The 288 message bits are the symbols immediately after the preamble.
        std::string messageBits = bits.substr(i + 24, 288);
        if (isInverted) {
            for (char& b : messageBits) b = (b == '1') ? '0' : '1';
        }

        if (checkCRC24Q(messageBits)) {
            if (decodeSubframe(messageBits, eph)) {
                // Map the symbol start back to absolute 1-ms tracking samples.
                subFrameStartIdx = firstSymbolSampleIdx + static_cast<int>(i) * 5;
                detectedTOW = eph.SOW;
                return true;
            }
        }
    }
    return false;
}

bool EphemerisDecoder::decodeSubframe(const std::string& navBitsBin, Ephemeris& eph)
{
    if (navBitsBin.length() < 288) {
        return false;
    }

    const double gpsPi = 3.1415926535898;

    int PRN = static_cast<int>(bin2dec(navBitsBin.substr(0, 6)));
    if (PRN < 1 || PRN > 63) {
        return false;
    }
    eph.PRN = PRN;

    int mesType = static_cast<int>(bin2dec(navBitsBin.substr(6, 6)));

    if (eph.SOW == 0.0) {
        eph.SOW = static_cast<double>(bin2dec(navBitsBin.substr(12, 18))) * 3.0;
    }

    switch (mesType)
    {
    case 10: // Ephemeris 1
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

    case 11: // Ephemeris 2
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

    case 30: // Clock, Ionosphere & Group Delay
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

        // T_GDB1Cp is intentionally NOT decoded here: MATLAB's ephemeris.m never
        // populates it either, so it stays at its struct default (0.0), which the
        // faithful satpos() subtracts from the clock correction (matching MATLAB).
        // The bit position used previously (substr(219, 12)) was made-up and wrong.
        break;
    }

    case 31:
    case 32:
    case 33:
    case 34:
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

bool EphemerisDecoder::findAllEphemerisFrames(const std::vector<double>& prompt_I, Ephemeris& eph, int& firstFrameStartIdx, double& firstTOW)
{
    const std::string PREAMBLE_NORM = "111000100100110111101000";
    const std::string PREAMBLE_INV = "000111011011001000010111";

    // Recover the 200-sps B-CNAV2 symbols from the 1-ms correlator stream.
    std::string bits;
    int firstSymbolSampleIdx = 0;
    if (!extractB2aSymbolBits(prompt_I, bits, firstSymbolSampleIdx)) return false;
    if (bits.size() < 24 + 288) return false;

    // Required message types for BDS-3 B2a: 10 (Eph 1), 11 (Eph 2), 30-34 (Clock)
    const std::set<int> requiredTypes = {10, 11, 30, 31, 32, 33, 34};
    std::set<int> foundTypes;
    bool gotClockType = false;

    int referenceFrameIdx = -1;
    double referenceTOW = 0.0;
    bool foundReference = false;

    // Search across the entire recovered symbol stream. B-CNAV2 rotates the
    // message types, so the full set (10, 11 and a clock type) is accumulated
    // over multiple frames. The 24-bit CRC protects against false matches.
    auto accumulateFrame = [&](const std::string& messageBits, int symbolIdx, bool isInverted) -> bool {
        std::string msg = messageBits;
        if (isInverted) {
            for (char& b : msg) b = (b == '1') ? '0' : '1';
        }
        if (!checkCRC24Q(msg)) return false;

        Ephemeris tempEph = eph;
        if (!decodeSubframe(msg, tempEph)) return false;

        for (int id : tempEph.idValid) {
            if (requiredTypes.count(id)) {
                foundTypes.insert(id);
                if (id >= 30 && id <= 34) gotClockType = true;
            }
        }

        if (!foundReference) {
            referenceFrameIdx = firstSymbolSampleIdx + symbolIdx * 5;
            referenceTOW = tempEph.SOW;
            foundReference = true;
        }

        if (tempEph.idValid[0] == 10) {
            eph.idValid[0] = 10;
            eph.WN = tempEph.WN;
            eph.DIF = tempEph.DIF;
            eph.SIF = tempEph.SIF;
            eph.AIF = tempEph.AIF;
            eph.t_oe = tempEph.t_oe;
            eph.SatType = tempEph.SatType;
            eph.deltaA = tempEph.deltaA;
            eph.ADot = tempEph.ADot;
            eph.delta_n_0 = tempEph.delta_n_0;
            eph.delta_n_0Dot = tempEph.delta_n_0Dot;
            eph.M_0 = tempEph.M_0;
            eph.e = tempEph.e;
            eph.omega = tempEph.omega;
        }
        if (tempEph.idValid[1] == 11) {
            eph.idValid[1] = 11;
            eph.HS = tempEph.HS;
            eph.DIF = tempEph.DIF;
            eph.SIF = tempEph.SIF;
            eph.AIF = tempEph.AIF;
            eph.omega_0 = tempEph.omega_0;
            eph.i_0 = tempEph.i_0;
            eph.omegaDot = tempEph.omegaDot;
            eph.i_0Dot = tempEph.i_0Dot;
            eph.C_is = tempEph.C_is;
            eph.C_ic = tempEph.C_ic;
            eph.C_rs = tempEph.C_rs;
            eph.C_rc = tempEph.C_rc;
            eph.C_us = tempEph.C_us;
            eph.C_uc = tempEph.C_uc;
        }
        for (int k = 2; k < 8; ++k) {
            if (tempEph.idValid[k] >= 30 && tempEph.idValid[k] <= 34) {
                eph.idValid[k] = tempEph.idValid[k];
                eph.t_oc = tempEph.t_oc;
                eph.a_0 = tempEph.a_0;
                eph.a_1 = tempEph.a_1;
                eph.a_2 = tempEph.a_2;
                eph.IODC_MSB2 = tempEph.IODC_MSB2;
                eph.IODC_LSB8 = tempEph.IODC_LSB8;
            }
        }

        bool has10 = foundTypes.count(10) != 0;
        bool has11 = foundTypes.count(11) != 0;
        return has10 && has11 && gotClockType && foundReference;
    };

    for (size_t i = 0; i + 24 + 288 <= bits.size(); ++i) {
        std::string pre = bits.substr(i, 24);
        bool isInverted = false;
        if (pre == PREAMBLE_NORM) isInverted = false;
        else if (pre == PREAMBLE_INV) isInverted = true;
        else continue;

        std::string messageBits = bits.substr(i + 24, 288);
        if (accumulateFrame(messageBits, static_cast<int>(i), isInverted)) {
            firstFrameStartIdx = referenceFrameIdx;
            firstTOW = referenceTOW;
            return true;
        }
    }

    bool has10 = foundTypes.count(10) != 0;
    bool has11 = foundTypes.count(11) != 0;
    if (has10 && has11 && gotClockType && foundReference) {
        firstFrameStartIdx = referenceFrameIdx;
        firstTOW = referenceTOW;
        return true;
    }

    return false;
}
