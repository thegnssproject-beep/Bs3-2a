#include "CheckPhase.h"

std::string CheckPhase::invertBits(const std::string& bits)
{
    std::string inverted = bits;
    for (char& c : inverted) {
        if (c == '0') c = '1';
        else if (c == '1') c = '0';
    }
    return inverted;
}

std::string CheckPhase::process(const std::string& word, char D30Star)
{
    std::string correctedWord = word;

    // If D30Star is '1', the first 24 data bits must be inverted
    if (D30Star == '1' && correctedWord.length() >= 24) {
        std::string dataBits = correctedWord.substr(0, 24);
        std::string invertedData = invertBits(dataBits);

        for (size_t i = 0; i < 24; ++i) {
            correctedWord[i] = invertedData[i];
        }
    }

    return correctedWord;
}

std::vector<int> CheckPhase::process(const std::vector<int>& word, int D30Star)
{
    std::vector<int> correctedWord = word;

    // Numerical version (1 represents bit value 1)
    if (D30Star == 1 && correctedWord.size() >= 24) {
        for (size_t i = 0; i < 24; ++i) {
            correctedWord[i] = (correctedWord[i] == 0) ? 1 : 0;
        }
    }

    return correctedWord;
}