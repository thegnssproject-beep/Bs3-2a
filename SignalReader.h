#pragma once

#include <vector>
#include <fstream>
#include <iostream>
#include "Settings.h"

class SignalReader
{
public:
    static bool loadSamples(const Settings& s, double msToLoad, std::vector<double>& I_samples, std::vector<double>& Q_samples)
    {
        std::ifstream file(s.fileName.toStdString(), std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Failed to open signal file: " << s.fileName.toStdString() << std::endl;
            return false;
        }

        // Seek past skip bytes
        file.seekg(s.skipNumberOfBytes, std::ios::beg);

        // Calculate sample count needed for requested milliseconds
        long long samplesToRead = static_cast<long long>(s.samplingFreq * (msToLoad / 1000.0));

        if (s.fileType == 1) {
            // 8-bit Real Samples (S0, S1, S2...)
            std::vector<int8_t> buffer(samplesToRead);
            file.read(reinterpret_cast<char*>(buffer.data()), samplesToRead);

            I_samples.resize(samplesToRead);
            Q_samples.assign(samplesToRead, 0.0); // No quadrature component

            for (size_t i = 0; i < buffer.size(); ++i) {
                I_samples[i] = static_cast<double>(buffer[i]);
            }
        }
        else if (s.fileType == 2) {
            // 8-bit I/Q Samples (I0, Q0, I1, Q1...)
            std::vector<int8_t> buffer(samplesToRead * 2);
            file.read(reinterpret_cast<char*>(buffer.data()), samplesToRead * 2);

            I_samples.resize(samplesToRead);
            Q_samples.resize(samplesToRead);

            for (long long i = 0; i < samplesToRead; ++i) {
                I_samples[i] = static_cast<double>(buffer[2 * i]);
                Q_samples[i] = static_cast<double>(buffer[2 * i + 1]);
            }
        }

        file.close();
        return true;
    }
};