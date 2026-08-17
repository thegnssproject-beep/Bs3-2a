#pragma once

#include <string>
#include <vector>

class CheckPhase
{
public:
    // Inverts binary string characters ('0' -> '1', '1' -> '0')
    static std::string invertBits(const std::string& bits);

    // Checks and corrects bit polarity for a string-based bit word
    static std::string process(const std::string& word, char D30Star);

    // Vector overload for integer/logical bit arrays (0 and 1)
    static std::vector<int> process(const std::vector<int>& word, int D30Star);
};