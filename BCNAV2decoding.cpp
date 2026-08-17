#include "BCNAV2decoding.h"

// Check CRC-24Q using generator polynomial: x^24 + x^23 + x^18 + x^17 + x^14 + x^11 + x^10 + x^7 + x^6 + x^5 + x^4 + x^3 + x + 1
bool BCNAV2Decoder::checkCRC24Q(const std::vector<int>& bits)
{
    if (bits.size() < 24) return false;

    uint32_t poly = 0x1864CFB; // CRC-24Q polynomial representation
    uint32_t crc = 0;

    for (size_t i = 0; i < bits.size(); ++i) {
        crc <<= 1;
        if (bits[i] ^ ((crc >> 24) & 1)) {
            crc ^= poly;
        }
    }
    crc &= 0xFFFFFF;
    return (crc == 0);
}

EphStructure BCNAV2Decoder::parseEphemerisMessage(const std::vector<int>& bits, EphStructure eph)
{
    if (bits.size() < 264) return eph;

    // Extract 6-bit message type (bits 1-6)
    int msgType = 0;
    for (int i = 0; i < 6; ++i) {
        msgType = (msgType << 1) | bits[i];
    }

    // Record decoded message type for validity checks
    if (msgType == 10) eph.idValid[0] = 10;
    else if (msgType == 11) eph.idValid[1] = 11;
    else if (msgType >= 30 && msgType <= 34) {
        eph.idValid[2] = msgType;
    }

    // Extract SOW / TOW (18-bit field)
    uint32_t sowBits = 0;
    for (int i = 6; i < 24; ++i) {
        sowBits = (sowBits << 1) | bits[i];
    }
    eph.SOW = static_cast<double>(sowBits) * 6.0; // 6-second message frame resolution

    return eph;
}

EphStructure BCNAV2Decoder::decode(const std::vector<double>& I_P_InputBits, double& firstSubFrame, double& TOW)
{
    EphStructure eph;
    firstSubFrame = std::numeric_limits<double>::infinity();
    TOW = std::numeric_limits<double>::infinity();

    if (I_P_InputBits.empty()) return eph;

    // 1. Antipodal secondary code [1, 1, 1, -1, 1]
    const int secondCode[5] = { 1, 1, 1, -1, 1 };

    // 2. Antipodal 24-bit preamble pattern
    const int preambleBits[24] = {
        -1, -1, -1,  1,  1,  1, -1,  1,  1, -1,  1,  1,
        -1, -1,  1, -1, -1, -1, -1,  1, -1,  1,  1,  1
    };

    // Upsample preamble by 5 to match 1 ms accumulation sample rate (120 ms total)
    std::vector<int> preambleMs(120);
    for (int i = 0; i < 24; ++i) {
        for (int j = 0; j < 5; ++j) {
            preambleMs[i * 5 + j] = preambleBits[i] * secondCode[j];
        }
    }

    // 3. Convert input tracking bits into hard +1 / -1 decisions
    size_t numSamples = I_P_InputBits.size();
    std::vector<int> bits(numSamples);
    for (size_t i = 0; i < numSamples; ++i) {
        bits[i] = (I_P_InputBits[i] > 0.0) ? 1 : -1;
    }

    // 4. Cross-correlation with preamble pattern
    std::vector<int> candidateIndices;
    size_t preambleLen = preambleMs.size();

    if (numSamples >= preambleLen) {
        for (size_t i = 0; i <= numSamples - preambleLen; ++i) {
            int corr = 0;
            for (size_t k = 0; k < preambleLen; ++k) {
                corr += bits[i + k] * preambleMs[k];
            }
            // Correlation threshold check (> 115 out of 120 max match)
            if (std::abs(corr) > 115) {
                candidateIndices.push_back(static_cast<int>(i));
            }
        }
    }

    // 5. B-CNAV2 Message Decoding Loop
    for (int idx : candidateIndices) {
        if (idx + 3000 <= static_cast<int>(bits.size())) {
            std::vector<int> navBits(600, 0);

            // Wipe off secondary code and group 5 ms accumulations into 1 data bit
            for (int b = 0; b < 600; ++b) {
                int sum = 0;
                for (int s = 0; s < 5; ++s) {
                    sum += bits[idx + b * 5 + s] * secondCode[s];
                }
                navBits[b] = (sum > 0) ? 1 : -1;
            }

            // Correct polarity if preamble is inverted
            bool polarityMatch = true;
            for (int k = 0; k < 24; ++k) {
                if (navBits[k] != preambleBits[k]) {
                    polarityMatch = false;
                    break;
                }
            }

            if (!polarityMatch) {
                for (int b = 0; b < 600; ++b) {
                    navBits[b] = -navBits[b];
                }
            }

            // Extract 288 payload bits past 24-bit preamble
            std::vector<int> decodedNavBits(288);
            std::vector<int> navDataLogic(288);
            for (int k = 0; k < 288; ++k) {
                decodedNavBits[k] = navBits[24 + k];
                navDataLogic[k] = (decodedNavBits[k] < 0) ? 1 : 0; // Convert to 0/1 logical bits
            }

            // Validate payload using CRC-24Q
            if (checkCRC24Q(navDataLogic)) {
                eph = parseEphemerisMessage(navDataLogic, eph);

                if (std::isinf(firstSubFrame)) {
                    firstSubFrame = static_cast<double>(idx);
                    TOW = eph.SOW;
                }
            }
        }
    }

    return eph;
}