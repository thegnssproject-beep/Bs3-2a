#include "CalcCNoPLD.h"

CNoPLDResult CalcCNoPLD::compute(const ChannelTrackResult& trackResults, const Settings& settings, int loopCnt)
{
    CNoPLDResult res;
    double T = settings.intTime;
    int interval = settings.CNoInterval;

    if (loopCnt < interval) {
        return res;
    }

    int startIdx = loopCnt - interval;

    // Helper: Mean calculation
    auto calcMean = [](const std::vector<double>& v) {
        double sum = std::accumulate(v.begin(), v.end(), 0.0);
        return sum / static_cast<double>(v.size());
        };

    // Helper: Sample Variance calculation
    auto calcVar = [&calcMean](const std::vector<double>& v) {
        if (v.size() <= 1) return 0.0;
        double mean = calcMean(v);
        double sq_sum = 0.0;
        for (double x : v) {
            sq_sum += (x - mean) * (x - mean);
        }
        return sq_sum / static_cast<double>(v.size() - 1);
        };

    // =========================================================================
    // 1. Data Channel C/N0 & PLL Lock Detector Estimation
    // =========================================================================
    std::vector<double> I_P(trackResults.I_P.begin() + startIdx, trackResults.I_P.begin() + loopCnt);
    std::vector<double> Q_P(trackResults.Q_P.begin() + startIdx, trackResults.Q_P.begin() + loopCnt);

    std::vector<double> Z(interval);
    for (int i = 0; i < interval; ++i) {
        Z[i] = I_P[i] * I_P[i] + Q_P[i] * Q_P[i];
    }

    double Zm = calcMean(Z);
    double Zv = calcVar(Z);

    double Pav = std::sqrt(std::max(0.0, Zm * Zm - Zv));
    double Nv = 0.5 * (Zm - Pav);

    double DataCNo = (Nv > 0.0) ? std::abs((1.0 / T) * Pav / (2.0 * Nv)) : 1.0;
    res.CNo[0] = 10.0 * std::log10(std::max(1.0, DataCNo));

    // Data Channel PLL Lock Detector (Narrowband vs Wideband Power Ratio)
    double sum_pos_I = 0.0, sum_neg_I = 0.0, sum_Q = 0.0;
    for (int i = 0; i < interval; ++i) {
        if (I_P[i] > 0.0) sum_pos_I += I_P[i];
        else sum_neg_I += I_P[i];
        sum_Q += Q_P[i];
    }

    double diff_I = sum_pos_I - sum_neg_I;
    double NBP_data = diff_I * diff_I + sum_Q * sum_Q;
    double NBD_data = diff_I * diff_I - sum_Q * sum_Q;

    res.PllDetector[0] = (NBP_data != 0.0) ? (NBD_data / NBP_data) : 0.0;

    // =========================================================================
    // 2. Pilot Channel C/N0 & PLL Lock Detector Estimation
    // =========================================================================
    double PilotCNo = 0.0;

    if (settings.pilotTRKflag) {
        std::vector<double> Pilot_Q_P(trackResults.Pilot_I_P.begin() + startIdx, trackResults.Pilot_I_P.begin() + loopCnt);
        std::vector<double> Pilot_I_P(trackResults.Pilot_Q_P.begin() + startIdx, trackResults.Pilot_Q_P.begin() + loopCnt);

        std::vector<double> Z_pilot(interval);
        for (int i = 0; i < interval; ++i) {
            Z_pilot[i] = Pilot_I_P[i] * Pilot_I_P[i] + Pilot_Q_P[i] * Pilot_Q_P[i];
        }

        double Zm_p = calcMean(Z_pilot);
        double Zv_p = calcVar(Z_pilot);

        double Pav_p = std::sqrt(std::max(0.0, Zm_p * Zm_p - Zv_p));
        double Nv_p = 0.5 * (Zm_p - Pav_p);

        PilotCNo = (Nv_p > 0.0) ? std::abs((1.0 / T) * Pav_p / (2.0 * Nv_p)) : 1.0;
        res.CNo[1] = 10.0 * std::log10(std::max(1.0, PilotCNo));

        // Pilot Channel PLL Lock Detector
        double sum_pos_Ip = 0.0, sum_neg_Ip = 0.0, sum_Qp = 0.0;
        for (int i = 0; i < interval; ++i) {
            if (Pilot_I_P[i] > 0.0) sum_pos_Ip += Pilot_I_P[i];
            else sum_neg_Ip += Pilot_I_P[i];
            sum_Qp += Pilot_Q_P[i];
        }

        double diff_Ip = sum_pos_Ip - sum_neg_Ip;
        double NBP_pilot = diff_Ip * diff_Ip + sum_Qp * sum_Qp;
        double NBD_pilot = diff_Ip * diff_Ip - sum_Qp * sum_Qp;

        res.PllDetector[1] = (NBP_pilot != 0.0) ? (NBD_pilot / NBP_pilot) : 0.0;
    }

    // Combined Total B2a C/N0
    res.CNo[2] = 10.0 * std::log10(std::max(1.0, DataCNo + PilotCNo));

    return res;
}