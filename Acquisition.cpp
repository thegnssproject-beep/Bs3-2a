#define _USE_MATH_DEFINES
#include <cmath>
#include "Acquisition.h"
#include <vector>
#include <complex>
#include <algorithm>
#include <QStringList>
#include <QString>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {
    // Exact Cooley-Tukey Radix-2 FFT
    void fft_radix2(std::vector<std::complex<double>>& x, bool invert) noexcept {
        size_t n = x.size();
        for (size_t i = 1, j = 0; i < n; i++) {
            size_t bit = n >> 1;
            for (; j & bit; bit >>= 1) j ^= bit;
            j ^= bit;
            if (i < j) std::swap(x[i], x[j]);
        }
        for (size_t len = 2; len <= n; len <<= 1) {
            double angle = 2.0 * M_PI / static_cast<double>(len) * (invert ? 1.0 : -1.0);
            std::complex<double> wlen(std::cos(angle), std::sin(angle));
            for (size_t i = 0; i < n; i += len) {
                std::complex<double> w(1.0, 0.0);
                for (size_t j = 0; j < len / 2; j++) {
                    std::complex<double> u = x[i + j];
                    std::complex<double> v = x[i + j + len / 2] * w;
                    x[i + j] = u + v;
                    x[i + j + len / 2] = u - v;
                    w *= wlen;
                }
            }
        }
        if (invert) {
            double invN = 1.0 / static_cast<double>(n);
            for (std::complex<double>& val : x) {
                val *= invN;
            }
        }
    }
}

// Exact Official BDS-3 B2a ICD G2 Register Initial State Vectors (PRN 1 to 63)
static const int B2a_reg2_initial_states[63][13] = {
    {1,0,0,0,0,0,0,1,0,0,1,0,1}, {1,0,0,0,0,0,0,1,1,0,1,0,0}, {1,0,0,0,0,1,0,1,0,1,1,0,1},
    {1,0,0,0,1,0,1,0,0,1,1,1,1}, {1,0,0,0,1,0,1,0,1,0,1,0,1}, {1,0,0,0,1,1,0,1,0,1,1,1,0},
    {1,0,0,0,1,1,1,1,0,1,1,1,0}, {1,0,0,0,1,1,1,1,1,1,0,1,1}, {1,0,0,1,1,0,0,1,0,1,0,0,1},
    {1,0,0,1,1,1,1,0,1,1,0,1,0}, {1,0,1,0,0,0,0,1,1,0,1,0,1}, {1,0,1,0,0,0,1,0,0,0,1,0,0},
    {1,0,1,0,0,0,1,0,1,0,1,0,1}, {1,0,1,0,0,0,1,0,1,1,0,1,1}, {1,0,1,0,0,0,1,0,1,1,1,0,0},
    {1,0,1,0,0,1,0,1,0,0,0,1,1}, {1,0,1,0,0,1,1,1,1,0,1,1,1}, {1,0,1,0,1,0,0,0,0,0,0,0,1},
    {1,0,1,0,1,0,0,1,1,1,1,1,0}, {1,0,1,0,1,1,0,1,0,1,0,1,1}, {1,0,1,0,1,1,0,1,1,0,0,0,1},
    {1,0,1,1,0,0,1,0,1,0,0,1,1}, {1,0,1,1,0,0,1,1,0,0,0,1,0}, {1,0,1,1,0,1,0,0,1,1,0,0,0},
    {1,0,1,1,0,1,0,1,1,0,1,1,0}, {1,0,1,1,0,1,1,1,1,0,0,1,0}, {1,0,1,1,0,1,1,1,1,1,1,1,1},
    {1,0,1,1,1,0,0,0,1,0,0,1,0}, {1,0,1,1,1,0,0,1,1,1,1,0,0}, {1,0,1,1,1,1,0,1,0,0,0,0,1},
    {1,0,1,1,1,1,1,0,0,1,0,0,0}, {1,0,1,1,1,1,1,0,1,0,1,0,0}, {1,0,1,1,1,1,1,1,0,1,0,1,1},
    {1,0,1,1,1,1,1,1,1,0,0,1,1}, {1,1,0,0,0,0,1,0,1,0,0,0,1}, {1,1,0,0,0,1,0,0,1,0,1,0,0},
    {1,1,0,0,0,1,0,1,1,0,1,1,1}, {1,1,0,0,1,0,0,0,1,0,0,0,1}, {1,1,0,0,1,0,0,0,1,1,0,0,1},
    {1,1,0,0,1,1,0,1,0,1,0,1,1}, {1,1,0,0,1,1,0,1,1,0,0,0,1}, {1,1,0,0,1,1,1,0,1,0,0,1,0},
    {1,1,0,1,0,0,1,0,1,0,1,0,1}, {1,1,0,1,0,0,1,1,1,0,1,0,0}, {1,1,0,1,0,1,1,0,0,1,0,1,1},
    {1,1,0,1,1,0,1,0,1,0,1,1,1}, {1,1,1,0,0,0,0,1,1,0,1,0,0}, {1,1,1,0,0,1,0,0,0,0,0,1,1},
    {1,1,1,0,0,1,0,0,0,1,0,1,1}, {1,1,1,0,0,1,0,1,0,0,0,1,1}, {1,1,1,0,0,1,0,1,0,1,0,0,0},
    {1,1,1,0,1,0,0,1,1,1,0,1,1}, {1,1,1,0,1,1,0,0,1,0,1,1,1}, {1,1,1,1,0,0,1,0,0,1,0,0,0},
    {1,1,1,1,0,1,0,0,1,0,1,0,0}, {1,1,1,1,0,1,0,0,1,1,0,0,1}, {1,1,1,1,0,1,1,0,1,1,0,1,0},
    {1,1,1,1,0,1,1,1,1,1,0,0,0}, {1,1,1,1,0,1,1,1,1,1,1,1,1}, {1,1,1,1,1,1,0,1,1,0,1,0,1},
    {0,0,1,0,0,0,0,0,0,0,0,1,0}, {1,1,0,1,1,1,1,1,1,0,1,0,1}, {0,0,0,1,1,1,1,0,1,0,0,1,0}
};

std::vector<double> Acquisition::generateB2aDataCode(int prn, const Settings& settings)
{
    int codeLength = settings.codeLength;
    std::vector<double> code(codeLength, 0.0);
    if (prn < 1 || prn > 63) return code;

    const int reg1_Taps[4] = { 0, 4, 10, 12 };
    const int reg2_Taps[6] = { 2, 4, 8, 10, 11, 12 };

    std::vector<double> reg1(13, -1.0);
    std::vector<double> reg2(13, 0.0);

    int prnIdx = prn - 1;
    for (int i = 0; i < 13; ++i) {
        reg2[i] = 1.0 - 2.0 * B2a_reg2_initial_states[prnIdx][i];
    }

    const int resetIndex = 8190;
    for (int ind = 1; ind <= codeLength; ++ind) {
        code[ind - 1] = reg1[12] * reg2[12];
        double feedback1 = reg1[reg1_Taps[0]] * reg1[reg1_Taps[1]] * reg1[reg1_Taps[2]] * reg1[reg1_Taps[3]];
        double feedback2 = reg2[reg2_Taps[0]] * reg2[reg2_Taps[1]] * reg2[reg2_Taps[2]] * reg2[reg2_Taps[3]] * reg2[reg2_Taps[4]] * reg2[reg2_Taps[5]];

        for (int i = 12; i > 0; --i) reg1[i] = reg1[i - 1];
        reg1[0] = feedback1;
        for (int i = 12; i > 0; --i) reg2[i] = reg2[i - 1];
        reg2[0] = feedback2;

        if (ind == resetIndex) {
            reg1.assign(13, -1.0);
        }
    }
    return code;
}

std::vector<double> Acquisition::generateB2aPilotCode(int prn, const Settings& settings)
{
    int codeLength = settings.codeLength;
    std::vector<double> code(codeLength, 0.0);
    if (prn < 1 || prn > 63) return code;

    const int reg1_Taps[4] = { 2, 5, 6, 12 };
    const int reg2_Taps[6] = { 0, 4, 6, 7, 11, 12 };

    std::vector<double> reg1(13, -1.0);
    std::vector<double> reg2(13, 0.0);

    int prnIdx = prn - 1;
    for (int i = 0; i < 13; ++i) {
        reg2[i] = 1.0 - 2.0 * B2a_reg2_initial_states[prnIdx][i];
    }

    const int resetIndex = 8190;
    for (int ind = 1; ind <= codeLength; ++ind) {
        code[ind - 1] = reg1[12] * reg2[12];
        double feedback1 = reg1[reg1_Taps[0]] * reg1[reg1_Taps[1]] * reg1[reg1_Taps[2]] * reg1[reg1_Taps[3]];
        double feedback2 = reg2[reg2_Taps[0]] * reg2[reg2_Taps[1]] * reg2[reg2_Taps[2]] * reg2[reg2_Taps[3]] * reg2[reg2_Taps[4]] * reg2[reg2_Taps[5]];

        for (int i = 12; i > 0; --i) reg1[i] = reg1[i - 1];
        reg1[0] = feedback1;
        for (int i = 12; i > 0; --i) reg2[i] = reg2[i - 1];
        reg2[0] = feedback2;

        if (ind == resetIndex) {
            reg1.assign(13, -1.0);
        }
    }
    return code;
}

AcqResults Acquisition::run(const std::vector<std::complex<double>>& signal, const Settings& settings)
{
    AcqResults results;
    results.carrFreq.assign(64, 0.0);
    results.codePhase.assign(64, 0.0);
    results.peakMetric.assign(64, 0.0);

    const double fs = settings.samplingFreq;
    const double ifFreq = settings.IF;
    const double ts = 1.0 / fs;

    const int samplesPerCode = static_cast<int>(std::round(fs / (settings.codeFreqBasis / settings.codeLength)));
    const size_t len2ms = samplesPerCode * 2;

    if (signal.size() < len2ms) {
        return results;
    }

    std::vector<int> targetPrns;
    QString rawList = settings.targetSatList.trimmed();
    if (rawList.isEmpty()) rawList = settings.acqSatelliteList.trimmed();

    if (rawList.contains("-") || rawList.contains(":")) {
        QString delim = rawList.contains("-") ? "-" : ":";
        QStringList parts = rawList.split(delim);
        if (parts.size() == 2) {
            int startPrn = parts[0].trimmed().toInt();
            int endPrn = parts[1].trimmed().toInt();
            for (int p = std::min(startPrn, endPrn); p <= std::max(startPrn, endPrn); ++p) {
                if (p >= 1 && p <= 63) targetPrns.push_back(p);
            }
        }
    }
    else {
        QString cleanList = rawList;
        cleanList.replace(",", " ");
        QStringList tokens = cleanList.split(" ", Qt::SkipEmptyParts);
        for (const QString& tok : tokens) {
            bool ok = false;
            int prn = tok.toInt(&ok);
            if (ok && prn >= 1 && prn <= 63) targetPrns.push_back(prn);
        }
    }

    if (targetPrns.empty()) {
        for (int p = 19; p <= 56; ++p) targetPrns.push_back(p);
    }

    size_t N = 1;
    while (N < len2ms) N <<= 1;

    std::complex<double> meanVal(0.0, 0.0);
    for (size_t i = 0; i < len2ms; ++i) meanVal += signal[i];
    meanVal /= static_cast<double>(len2ms);

    std::vector<std::complex<double>> sig2ms(len2ms);
    for (size_t i = 0; i < len2ms; ++i) sig2ms[i] = signal[i] - meanVal;

    std::vector<std::vector<std::complex<double>>> dataFfts(64), pilotFfts(64);
    for (int prn : targetPrns) {
        if (settings.cancelRequested && settings.cancelRequested->load()) break;
        dataFfts[prn].assign(N, { 0.0, 0.0 });
        pilotFfts[prn].assign(N, { 0.0, 0.0 });

        std::vector<double> codeData = generateB2aDataCode(prn, settings);
        std::vector<double> codePilot = generateB2aPilotCode(prn, settings);

        for (int i = 0; i < samplesPerCode; ++i) {
            int idx = static_cast<int>(std::floor(((i + 1) * ts) / (1.0 / settings.codeFreqBasis))) % settings.codeLength;
            dataFfts[prn][i] = { codeData[idx], 0.0 };
            pilotFfts[prn][i] = { codePilot[idx], 0.0 };
        }

        fft_radix2(dataFfts[prn], false);
        fft_radix2(pilotFfts[prn], false);
        for (size_t i = 0; i < N; ++i) {
            dataFfts[prn][i] = std::conj(dataFfts[prn][i]);
            pilotFfts[prn][i] = std::conj(pilotFfts[prn][i]);
        }
    }

    const int numberOfFrqBins = static_cast<int>(std::round(settings.acqSearchBand * 2.0 / settings.acqStep)) + 1;
    std::vector<std::vector<std::complex<double>>> basebandBins(numberOfFrqBins, std::vector<std::complex<double>>(N, { 0.0, 0.0 }));
    std::vector<double> frqBins(numberOfFrqBins, 0.0);

    for (int frqBinIndex = 0; frqBinIndex < numberOfFrqBins; ++frqBinIndex) {
        frqBins[frqBinIndex] = ifFreq - settings.acqSearchBand + settings.acqStep * frqBinIndex;
        for (size_t n = 0; n < len2ms; ++n) {
            double phase = 2.0 * M_PI * frqBins[frqBinIndex] * (n * ts);
            std::complex<double> sigCarr(std::cos(phase), std::sin(phase));

            // Match MATLAB: IQfreqDom = fft(real(sigCarr .* sig2ms) + 1i*imag(sigCarr .* sig2ms))
            double I = sigCarr.real() * sig2ms[n].real() - sigCarr.imag() * sig2ms[n].imag();
            double Q = sigCarr.real() * sig2ms[n].imag() + sigCarr.imag() * sig2ms[n].real();
            basebandBins[frqBinIndex][n] = std::complex<double>(I, Q);
        }
        fft_radix2(basebandBins[frqBinIndex], false);
    }

    const int samples2CodeChip = static_cast<int>(std::ceil(fs / settings.codeFreqBasis)) * 2;

    for (int prn : targetPrns) {
        if (settings.cancelRequested && settings.cancelRequested->load()) break;
        if (dataFfts[prn].empty()) continue;

        double maxPeak = 0.0;
        int bestFrqIdx = 0;
        int bestCodePhase = 0;

        for (int f = 0; f < numberOfFrqBins; ++f) {
            std::vector<std::complex<double>> convData(N), convPilot(N);
            for (size_t i = 0; i < N; ++i) {
                convData[i] = basebandBins[f][i] * dataFfts[prn][i];
                convPilot[i] = basebandBins[f][i] * pilotFfts[prn][i];
            }

            fft_radix2(convData, true);
            fft_radix2(convPilot, true);

            for (size_t tau = 0; tau < len2ms; ++tau) {
                double pwr = std::abs(convData[tau]) + std::abs(convPilot[tau]);
                if (pwr > maxPeak) {
                    maxPeak = pwr;
                    bestFrqIdx = f;
                    bestCodePhase = static_cast<int>(tau);
                }
            }
        }

        // Reconstruct best Doppler slice
        std::vector<std::complex<double>> convData(N), convPilot(N);
        for (size_t i = 0; i < N; ++i) {
            convData[i] = basebandBins[bestFrqIdx][i] * dataFfts[prn][i];
            convPilot[i] = basebandBins[bestFrqIdx][i] * pilotFfts[prn][i];
        }
        fft_radix2(convData, true);
        fft_radix2(convPilot, true);

        std::vector<double> bestRow(len2ms, 0.0);
        for (size_t tau = 0; tau < len2ms; ++tau) {
            bestRow[tau] = std::abs(convData[tau]) + std::abs(convPilot[tau]);
        }

        // Exact MATLAB Exclusion Range (1-based to 0-based boundary translation)
        int codePhase1Based = bestCodePhase + 1;
        int excludeRangeIndex1 = codePhase1Based - samples2CodeChip;
        int excludeRangeIndex2 = codePhase1Based + samples2CodeChip;
        int excludeRangeIndex3 = codePhase1Based - samplesPerCode + samples2CodeChip;
        int excludeRangeIndex4 = codePhase1Based + samplesPerCode - samples2CodeChip;

        std::vector<int> codePhaseRange;
        if (excludeRangeIndex1 >= 1) {
            int lStart = std::max(1, excludeRangeIndex3);
            for (int i = lStart; i <= excludeRangeIndex1; ++i) codePhaseRange.push_back(i - 1);
        }
        if (excludeRangeIndex2 <= static_cast<int>(len2ms)) {
            int rEnd = std::min(excludeRangeIndex4, static_cast<int>(len2ms));
            for (int i = excludeRangeIndex2; i <= rEnd; ++i) codePhaseRange.push_back(i - 1);
        }

        double secondPeakSize = 0.0;
        for (int idx : codePhaseRange) {
            if (idx >= 0 && idx < static_cast<int>(len2ms)) {
                if (bestRow[idx] > secondPeakSize) {
                    secondPeakSize = bestRow[idx];
                }
            }
        }

        double metric = (secondPeakSize > 1e-12) ? (maxPeak / secondPeakSize) : 1.0;
        results.peakMetric[prn] = metric;
        results.carrFreq[prn] = frqBins[bestFrqIdx];
        results.codePhase[prn] = static_cast<double>(bestCodePhase % samplesPerCode);

        // Fine resolution frequency search (matches MATLAB acquisition.m lines 256-335).
        // The coarse 400 Hz grid can leave up to +/-acqStep/2 Hz of carrier error,
        // which is beyond the Costas PLL pull-in and would prevent lock (ring scatter).
        // Refine with 25 Hz steps over a band centered on the coarse bin.
        if (metric > settings.acqThreshold && bestCodePhase >= 0 &&
            static_cast<size_t>(bestCodePhase + settings.fineNoncoh * samplesPerCode) <= signal.size()) {
            const int numFineBins = static_cast<int>(std::round(settings.acqStep / 25.0)) + 1;

            // Sample the data and pilot codes at the sampling grid (same as coarse local codes)
            std::vector<double> longDataCode(settings.fineNoncoh * samplesPerCode);
            std::vector<double> longPilotCode(settings.fineNoncoh * samplesPerCode);
            {
                std::vector<double> baseData = generateB2aDataCode(prn, settings);
                std::vector<double> basePilot = generateB2aPilotCode(prn, settings);
                for (int j = 0; j < settings.fineNoncoh * samplesPerCode; ++j) {
                    int idx = static_cast<int>(std::floor(((j + 1) * ts) / (1.0 / settings.codeFreqBasis)))
                        % settings.codeLength;
                    longDataCode[j] = baseData[idx];
                    longPilotCode[j] = basePilot[idx];
                }
            }

            double bestFineFreq = frqBins[bestFrqIdx];
            double bestFine = -1.0;
            for (int bin = 0; bin < numFineBins; ++bin) {
                double fineFreq = frqBins[bestFrqIdx] - settings.acqStep / 2.0 + 25.0 * bin;
                std::vector<std::complex<double>> sumData(settings.fineNoncoh, { 0.0, 0.0 });
                std::vector<std::complex<double>> sumPilot(settings.fineNoncoh, { 0.0, 0.0 });
                for (int j = 0; j < settings.fineNoncoh * samplesPerCode; ++j) {
                    double phase = 2.0 * M_PI * fineFreq * (j * ts);
                    std::complex<double> carr(std::cos(phase), std::sin(phase));
                    int perCode = j / samplesPerCode;
                    sumData[perCode] += longDataCode[j] * carr * signal[bestCodePhase + j];
                    sumPilot[perCode] += longPilotCode[j] * carr * signal[bestCodePhase + j];
                }
                double finePower = 0.0;
                for (int k = 0; k < settings.fineNoncoh; ++k) {
                    finePower += std::abs(sumData[k]) + std::abs(sumPilot[k]);
                }
                if (finePower > bestFine) {
                    bestFine = finePower;
                    bestFineFreq = fineFreq;
                }
            }
            results.carrFreq[prn] = bestFineFreq;
        }
    }

    return results;
}