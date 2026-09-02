#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

#include "Tracking.h"
#include "Acquisition.h"
#include "CalcCNoPLD.h"
#include <cmath>
#include <iostream>
#include <algorithm>
#include <vector>
#include <complex>
#include <cstdlib>
#include <fstream>
#include <string>
#include <QCoreApplication>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void Tracking::calcLoopCoef(double noiseBW, double dampingRatio, double k, double& tau1, double& tau2) {
    double wn = noiseBW * 8.0 * dampingRatio / (4.0 * dampingRatio * dampingRatio + 1.0);
    tau1 = k / (wn * wn);
    tau2 = 2.0 * dampingRatio / wn;
}

void Tracking::calcLoopCoefCarr(const Settings& s, double& pf3, double& pf2, double& pf1) {
    double a3 = 2.0;
    double b3 = 2.0;
    double Wn = 1.2 * s.pllNoiseBandwidth;

    pf3 = std::pow(Wn, 3) * std::pow(s.intTime, 2);
    pf2 = a3 * std::pow(Wn, 2) * s.intTime;
    pf1 = b3 * Wn;
}

std::vector<ChannelTrackResult> Tracking::run(std::ifstream& fid, std::vector<Channel>& channels, const Settings& settings)
{
    int codePeriods = std::min<int>(static_cast<int>(settings.msToProcess), 49000);
    std::vector<ChannelTrackResult> trackResults(channels.size());

    double tau1code = 0.0, tau2code = 0.0;
    calcLoopCoef(settings.dllNoiseBandwidth, settings.dllDampingRatio, 1.0, tau1code, tau2code);
    double PDIcode = settings.intTime;

    double pf3 = 0.0, pf2 = 0.0, pf1 = 0.0;
    calcLoopCoefCarr(settings, pf3, pf2, pf1);

    int dataAdaptCoeff = (settings.fileType == 1) ? 1 : 2;
    double earlyLateSpc = settings.dllCorrelatorSpacing;

    for (size_t channelNr = 0; channelNr < channels.size(); ++channelNr)
    {
        if (settings.cancelRequested && settings.cancelRequested->load()) {
            break;
        }
        if (channels[channelNr].PRN == 0) continue;

        int prn = channels[channelNr].PRN;
        ChannelTrackResult& res = trackResults[channelNr];
        res.PRN = prn;
        res.status = 1;

        res.absoluteSample.resize(codePeriods, 0.0);
        res.codeFreq.resize(codePeriods, 0.0);
        res.carrFreq.resize(codePeriods, 0.0);
        res.remCodePhase.resize(codePeriods, 0.0);
        res.I_E.resize(codePeriods, 0.0); res.I_P.resize(codePeriods, 0.0); res.I_L.resize(codePeriods, 0.0);
        res.Q_E.resize(codePeriods, 0.0); res.Q_P.resize(codePeriods, 0.0); res.Q_L.resize(codePeriods, 0.0);
        res.dllError.resize(codePeriods, 0.0); res.dllDiscrFilt.resize(codePeriods, 0.0);
        res.pllError.resize(codePeriods, 0.0); res.pllDiscrFilt.resize(codePeriods, 0.0);
        res.CNo.resize(codePeriods, 0.0);

        if (settings.pilotTRKflag) {
            res.Pilot_I_E.resize(codePeriods, 0.0); res.Pilot_I_P.resize(codePeriods, 0.0); res.Pilot_I_L.resize(codePeriods, 0.0);
            res.Pilot_Q_E.resize(codePeriods, 0.0); res.Pilot_Q_P.resize(codePeriods, 0.0); res.Pilot_Q_L.resize(codePeriods, 0.0);
        }

        std::vector<double> baseDataCode = Acquisition::generateB2aDataCode(prn, settings);
        std::vector<double> B2aData(settings.codeLength + 2);
        B2aData[0] = baseDataCode[settings.codeLength - 1];
        for (int i = 0; i < settings.codeLength; ++i) B2aData[i + 1] = baseDataCode[i];
        B2aData[settings.codeLength + 1] = baseDataCode[0];

        std::vector<double> basePilotCode = Acquisition::generateB2aPilotCode(prn, settings);
        std::vector<double> B2aPilot(settings.codeLength + 2);
        if (settings.pilotTRKflag) {
            B2aPilot[0] = basePilotCode[settings.codeLength - 1];
            for (int i = 0; i < settings.codeLength; ++i) B2aPilot[i + 1] = basePilotCode[i];
            B2aPilot[settings.codeLength + 1] = basePilotCode[0];
        }

        double codeFreqBasis = settings.codeFreqBasis;
        double codeFreq = settings.codeFreqBasis;
        double remCodePhase = 0.0;

        double carrFreqBasis = channels[channelNr].acquiredFreq;
        double carrFreq = channels[channelNr].acquiredFreq;
        double remCarrPhase = 0.0;

        double oldCodeNco = 0.0;
        double oldCodeError = 0.0;
        double d2CarrError = 0.0;
        double dCarrError = 0.0;

        // Previous raw C/N0 estimate, used for the 50/50 smoothing that MATLAB
        // tracking.m applies (CNo*0.5 + prevCNo*0.5). Negative => no previous yet.
        double prevCNo = -1.0;

        std::ofstream dbgFile;
        bool dumpDebug = (std::getenv("BS32A_DEBUG") != nullptr);
        if (dumpDebug) {
            dbgFile.open("tracking_debug_PRN" + std::to_string(prn) + ".csv", std::ios::trunc);
            dbgFile << "loopCnt,absoluteSample,I_P,Q_P,I_E,I_L,Q_E,Q_L,pllError,carrNco,carrFreq,codeError,codeNco,codeFreq,remCodePhase,blksize\n";
        }

        // Reusable per-ms buffers (resized only when the block grows) to avoid
        // malloc/free churn on every integration. Numerically identical to
        // allocating fresh each iteration because buffers are fully overwritten
        // before use.
        std::vector<int8_t> rawBuffer;
        std::vector<double> carrsig_cos, carrsig_sin;
        std::vector<double> iBaseband, qBaseband;

        fid.clear();
        long long startByteOffset = static_cast<long long>(dataAdaptCoeff * (settings.skipNumberOfBytes + channels[channelNr].codePhase));
        fid.seekg(startByteOffset, std::ios::beg);

        for (int loopCnt = 0; loopCnt < codePeriods; ++loopCnt)
        {
            if (loopCnt % 200 == 0) {
                QCoreApplication::processEvents();
                if (settings.cancelRequested && settings.cancelRequested->load()) {
                    break;
                }
            }

            res.absoluteSample[loopCnt] = static_cast<double>(fid.tellg()) / static_cast<double>(dataAdaptCoeff);

            double codePhaseStep = codeFreq / settings.samplingFreq;
            int blksize = static_cast<int>(std::ceil((settings.codeLength - remCodePhase) / codePhaseStep));

            long long bytesToRead = dataAdaptCoeff * blksize;
            if (rawBuffer.size() < static_cast<size_t>(bytesToRead)) {
                rawBuffer.resize(bytesToRead);
            }
            fid.read(reinterpret_cast<char*>(rawBuffer.data()), bytesToRead);
            if (fid.gcount() != bytesToRead) break;

            if (carrsig_cos.size() < static_cast<size_t>(blksize)) {
                carrsig_cos.resize(blksize);
                carrsig_sin.resize(blksize);
            }
            for (int i = 0; i < blksize; ++i) {
                double trigarg = (carrFreq * 2.0 * M_PI * (static_cast<double>(i) / settings.samplingFreq)) + remCarrPhase;
                carrsig_cos[i] = std::cos(trigarg);
                carrsig_sin[i] = std::sin(trigarg);
            }
            double finalTrigarg = (carrFreq * 2.0 * M_PI * (static_cast<double>(blksize) / settings.samplingFreq)) + remCarrPhase;
            remCarrPhase = std::fmod(finalTrigarg, 2.0 * M_PI);
            if (remCarrPhase < 0.0) remCarrPhase += 2.0 * M_PI;

            // MATLAB mixing: carrsig = exp(+j*theta); qBaseband = real, iBaseband = imag (tracking.m)
            if (iBaseband.size() < static_cast<size_t>(blksize)) {
                iBaseband.resize(blksize);
                qBaseband.resize(blksize);
            }
            for (int i = 0; i < blksize; ++i) {
                double rawReal = static_cast<double>(rawBuffer[dataAdaptCoeff * i]);
                double rawImag = (dataAdaptCoeff == 2) ? static_cast<double>(rawBuffer[dataAdaptCoeff * i + 1]) : 0.0;

                qBaseband[i] = rawReal * carrsig_cos[i] - rawImag * carrsig_sin[i];
                iBaseband[i] = rawReal * carrsig_sin[i] + rawImag * carrsig_cos[i];
            }

            double I_E = 0.0, I_P = 0.0, I_L = 0.0;
            double Q_E = 0.0, Q_P = 0.0, Q_L = 0.0;
            double pilot_I_E = 0.0, pilot_I_P = 0.0, pilot_I_L = 0.0;
            double pilot_Q_E = 0.0, pilot_Q_P = 0.0, pilot_Q_L = 0.0;

            for (int i = 0; i < blksize; ++i) {
                double tcode_prompt = remCodePhase + i * codePhaseStep;
                double tcode_early = tcode_prompt - earlyLateSpc;
                double tcode_late = tcode_prompt + earlyLateSpc;

                int idxE = static_cast<int>(std::ceil(tcode_early));
                int idxP = static_cast<int>(std::ceil(tcode_prompt));
                int idxL = static_cast<int>(std::ceil(tcode_late));

                idxE = std::max(0, std::min(idxE, settings.codeLength + 1));
                idxP = std::max(0, std::min(idxP, settings.codeLength + 1));
                idxL = std::max(0, std::min(idxL, settings.codeLength + 1));

                double cE = B2aData[idxE], cP = B2aData[idxP], cL = B2aData[idxL];

                I_E += cE * iBaseband[i]; Q_E += cE * qBaseband[i];
                I_P += cP * iBaseband[i]; Q_P += cP * qBaseband[i];
                I_L += cL * iBaseband[i]; Q_L += cL * qBaseband[i];

                if (settings.pilotTRKflag) {
                    double pE = B2aPilot[idxE], pP = B2aPilot[idxP], pL = B2aPilot[idxL];
                    pilot_I_E += pE * iBaseband[i]; pilot_Q_E += pE * qBaseband[i];
                    pilot_I_P += pP * iBaseband[i]; pilot_Q_P += pP * qBaseband[i];
                    pilot_I_L += pL * iBaseband[i]; pilot_Q_L += pL * qBaseband[i];
                }
            }

            remCodePhase = (remCodePhase + blksize * codePhaseStep) - settings.codeLength;

            // Two-Quadrant Costas Loop Discriminator (data + pilot average, matching tracking.m)
            double carrError = std::atan(Q_P / (std::abs(I_P) > 1e-12 ? I_P : 1e-12)) / (2.0 * M_PI);
            if (settings.pilotTRKflag) {
                double realQI = pilot_Q_P;
                double imagQI = -pilot_I_P;
                double carrErrorQ = std::atan(imagQI / (std::abs(realQI) > 1e-12 ? realQI : 1e-12)) / (2.0 * M_PI);
                carrError = (carrError + carrErrorQ) * 0.5;
            }

            // 3rd-Order PLL Loop Filter Matching MATLAB
            d2CarrError += carrError * pf3;
            dCarrError += d2CarrError + carrError * pf2;
            double carrNco = dCarrError + carrError * pf1;

            res.carrFreq[loopCnt] = carrFreq;
            carrFreq = carrFreqBasis + carrNco;

            // DLL Code Discriminator (data + pilot average, matching tracking.m)
            double envE = std::sqrt(I_E * I_E + Q_E * Q_E);
            double envL = std::sqrt(I_L * I_L + Q_L * Q_L);
            double codeError = (envE - envL) / (envE + envL > 1e-12 ? (envE + envL) : 1e-12);
            if (settings.pilotTRKflag) {
                double pEnvE = std::sqrt(pilot_I_E * pilot_I_E + pilot_Q_E * pilot_Q_E);
                double pEnvL = std::sqrt(pilot_I_L * pilot_I_L + pilot_Q_L * pilot_Q_L);
                double codeErrorQ = (pEnvE - pEnvL) / (pEnvE + pEnvL > 1e-12 ? (pEnvE + pEnvL) : 1e-12);
                codeError = (codeError + codeErrorQ) * 0.5;
            }

            double codeNco = oldCodeNco + (tau2code / tau1code) * (codeError - oldCodeError) + codeError * (PDIcode / tau1code);
            oldCodeNco = codeNco;
            oldCodeError = codeError;

            res.codeFreq[loopCnt] = codeFreq;
            codeFreq = codeFreqBasis - codeNco;

            // Save Telemetry
            res.I_E[loopCnt] = I_E; res.I_P[loopCnt] = I_P; res.I_L[loopCnt] = I_L;
            res.Q_E[loopCnt] = Q_E; res.Q_P[loopCnt] = Q_P; res.Q_L[loopCnt] = Q_L;
            res.remCodePhase[loopCnt] = remCodePhase;
            res.pllError[loopCnt] = carrError;
            res.pllDiscrFilt[loopCnt] = carrNco;
            res.dllError[loopCnt] = codeError;
            res.dllDiscrFilt[loopCnt] = codeNco;

            if (settings.pilotTRKflag) {
                res.Pilot_I_P[loopCnt] = pilot_I_P;
                res.Pilot_Q_P[loopCnt] = pilot_Q_P;
            }

            // C/N0 Block Estimator (VSM): data + pilot combined B2a (CalcCNoPLD),
            // with the same 50/50 smoothing as MATLAB tracking.m (CNo*0.5 + prev*0.5).
            int cnoInterval = (settings.CNoInterval > 0) ? settings.CNoInterval : 200;
            if ((loopCnt + 1) % cnoInterval == 0) {
                CNoPLDResult cnoResult = CalcCNoPLD::compute(res, settings, loopCnt);
                double rawCNo = (settings.pilotTRKflag) ? cnoResult.CNo[2] : cnoResult.CNo[0];
                double epochCNo = (prevCNo >= 0.0) ? 0.5 * rawCNo + 0.5 * prevCNo : rawCNo;
                prevCNo = rawCNo;

                int startIdx = loopCnt + 1 - cnoInterval;
                for (int k = startIdx; k <= loopCnt; ++k) {
                    res.CNo[k] = epochCNo;
                }
            }

            if (dumpDebug) {
                dbgFile << loopCnt << "," << res.absoluteSample[loopCnt] << ","
                    << I_P << "," << Q_P << "," << I_E << "," << I_L << "," << Q_E << "," << Q_L << ","
                    << carrError << "," << carrNco << "," << res.carrFreq[loopCnt] << ","
                    << codeError << "," << codeNco << "," << res.codeFreq[loopCnt] << ","
                    << remCodePhase << "," << blksize << "\n";
            }
        }
    }

    return trackResults;
}