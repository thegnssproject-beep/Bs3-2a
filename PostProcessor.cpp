#include "PostProcessor.h"
#include "PreRun.h"
#include "Acquisition.h"
#include "Tracking.h"
#include "PostNavigation.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <complex>
#include <algorithm>
#include <QCoreApplication>

static void diagLog(const std::string& msg)
{
    std::ofstream out("C:/Yousuf_B2a/BS-3-2a_GUI/nav_diag.txt", std::ios::app);
    out << msg << "\n";
}

bool PostProcessor::runPipeline(const Settings& settings, SDRPipelineResults& results, Logger log)
{
    return runPipeline(settings, results, log, nullptr);
}

bool PostProcessor::runPipeline(const Settings& settings, SDRPipelineResults& results, Logger log, StageCallback onStage)
{
    auto emitLog = [&](const std::string& msg) {
        if (log) {
            log(msg);
            QCoreApplication::processEvents();
        }
        };

    auto emitStage1 = [&](const AcqResults& acq, const std::vector<int>& prns) {
        if (onStage) {
            PipelineStage st;
            st.stage = 1;
            st.acqResults = acq;
            st.trackedPrns = prns;
            onStage(st);
        }
        };
    auto emitStage2 = [&](const std::vector<ChannelTrackResult>& trk) {
        if (onStage) {
            PipelineStage st;
            st.stage = 2;
            st.trackResults = trk;
            onStage(st);
        }
        };
    auto emitStage3 = [&](const NavSolutions& nav) {
        if (onStage) {
            PipelineStage st;
            st.stage = 3;
            st.navSolutions = nav;
            onStage(st);
        }
        };

    // -------------------------------------------------------------
    // Record Start Time & Timestamp
    // -------------------------------------------------------------
    auto startTime = std::chrono::high_resolution_clock::now();
    std::time_t start_time_t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    std::tm tm_buf{};
#if defined(_WIN32) || defined(_WIN64)
    localtime_s(&tm_buf, &start_time_t);
#else
    localtime_r(&start_time_t, &tm_buf);
#endif

    std::ostringstream startBanner;
    startBanner << "=============================================================\n"
        << " BDS-3 B2a SDR GNSS Pipeline Execution Started\n"
        << " Timestamp: " << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S") << "\n"
        << " Signal File: " << settings.fileName.toStdString() << "\n"
        << " Duration to Process: " << settings.msToProcess << " ms\n"
        << "=============================================================\n";
    emitLog(startBanner.str());

    // 1. Validate File Access
    diagLog("PP: validate file open");
    std::ifstream testFid(settings.fileName.toStdString(), std::ios::binary);
    if (!testFid.is_open()) {
        emitLog("[ERROR] Cannot open raw signal file: " + settings.fileName.toStdString());
        return false;
    }
    diagLog("PP: file open ok");

    emitLog("Probing data (" + settings.fileName.toStdString() + ")...");
    emitLog("  Raw IF data ready for processing.\n");
    emitLog("Initiating GNSS SDR processing...");

    // 2. Read 2 ms for Acquisition
    diagLog("PP: read acquisition buffer start");
    int dataAdaptCoeff = (settings.fileType == 1) ? 1 : 2;
    testFid.seekg(dataAdaptCoeff * settings.skipNumberOfBytes, std::ios::beg);

    long long samplesPerCode = static_cast<long long>(std::round(
        settings.samplingFreq / (settings.codeFreqBasis / settings.codeLength)
    ));

    // Read enough data for the coarse 2 ms search window plus the MATLAB-style
    // fine frequency search (settings.fineNoncoh code periods starting at the
    // coarse code phase). The coarse window occupies the first 2 code periods.
    long long fineSamples = static_cast<long long>(settings.fineNoncoh) * samplesPerCode;
    long long totalSamples = 2 * samplesPerCode + fineSamples;
    long long samplesToRead = totalSamples;
    long long bytesToRead = dataAdaptCoeff * samplesToRead;

    std::vector<int8_t> rawBuffer(bytesToRead);
    testFid.read(reinterpret_cast<char*>(rawBuffer.data()), bytesToRead);
    std::streamsize bytesRead = testFid.gcount();
    testFid.close();
    diagLog("PP: read acquisition buffer done bytesRead=" + std::to_string(bytesRead));

    long long actualSamples = (bytesRead > 0) ? (bytesRead / dataAdaptCoeff) : 0;
    std::vector<std::complex<double>> acqSignal(actualSamples);

    if (dataAdaptCoeff == 1) {
        for (long long i = 0; i < actualSamples; ++i) {
            acqSignal[i] = std::complex<double>(static_cast<double>(rawBuffer[i]), 0.0);
        }
    }
    else {
        for (long long i = 0; i < actualSamples; ++i) {
            acqSignal[i] = std::complex<double>(static_cast<double>(rawBuffer[2 * i]), static_cast<double>(rawBuffer[2 * i + 1]));
        }
    }
    diagLog("PP: acqSignal built size=" + std::to_string(acqSignal.size()));

    // 3. Acquisition Stage
    auto t_acq_start = std::chrono::high_resolution_clock::now();
    emitLog("Acquisition started for BDS-3 B2a signals...");
    diagLog("PP: calling Acquisition::run");

    results.acqResults = Acquisition::run(acqSignal, settings);

    diagLog("PP: Acquisition::run returned");
    auto t_acq_end = std::chrono::high_resolution_clock::now();
    double acqElapsed = std::chrono::duration<double>(t_acq_end - t_acq_start).count();

    diagLog("PP: building activeChannels start");
    std::vector<Channel> activeChannels;
    size_t maxPrn = std::min<size_t>(results.acqResults.carrFreq.size(), results.acqResults.peakMetric.size());
    diagLog("PP: maxPrn=" + std::to_string(maxPrn) + " carrFreq.size=" + std::to_string(results.acqResults.carrFreq.size()) + " peakMetric.size=" + std::to_string(results.acqResults.peakMetric.size()) + " codePhase.size=" + std::to_string(results.acqResults.codePhase.size()));

    auto makeChannel = [&](int prn) noexcept -> Channel {
        Channel ch;
        ch.PRN = prn;
        diagLog("PP: makeChannel PRN=" + std::to_string(prn) + " start");
        ch.acquiredFreq = (prn < static_cast<int>(results.acqResults.carrFreq.size()) && results.acqResults.carrFreq[prn] != 0.0)
            ? results.acqResults.carrFreq[prn] : settings.IF;
        ch.codePhase = (prn < static_cast<int>(results.acqResults.codePhase.size()))
            ? results.acqResults.codePhase[prn] : 0.0;
        ch.status = 'T';
        diagLog("PP: makeChannel PRN=" + std::to_string(prn) + " done");
        return ch;
        };

    std::ostringstream acqSummary;
    acqSummary << "\n  --- Acquisition Search Summary (Finished in "
        << std::fixed << std::setprecision(2) << acqElapsed << " s) ---\n";
    acqSummary << "  PRN | Status     | Doppler (Hz) | Code Phase | Metric\n";
    acqSummary << "  ----+------------+--------------+------------+--------\n";

    diagLog("PP: for loop over PRNs start");
    for (size_t p = 1; p < maxPrn && p <= 63; ++p) {
        diagLog("PP: loop p=" + std::to_string(p));
        double metric = results.acqResults.peakMetric[p];
        if (metric >= 1.0) {
            diagLog("PP: metric PRN=" + std::to_string(p) + " val=" + std::to_string(metric));
        }
        if (metric >= settings.acqThreshold) {
            diagLog("PP: pushing PRN=" + std::to_string(p));
            activeChannels.push_back(makeChannel(static_cast<int>(p)));
            double doppler = results.acqResults.carrFreq[p] - settings.IF;
            acqSummary << "   " << std::setw(2) << p << " | ACQUIRED   | "
                << std::setw(12) << std::fixed << std::setprecision(1) << doppler << " | "
                << std::setw(10) << static_cast<int>(results.acqResults.codePhase[p]) << " | "
                << std::setw(6) << std::setprecision(2) << metric << "\n";
        }
    }
    diagLog("PP: for loop over PRNs done");

    emitLog(acqSummary.str());
    diagLog("PP: acqSummary logged");

    std::sort(activeChannels.begin(), activeChannels.end(),
        [&results](const Channel& a, const Channel& b) noexcept {
            return results.acqResults.peakMetric[a.PRN] > results.acqResults.peakMetric[b.PRN];
        });
    diagLog("PP: sort done");

    int limit = (settings.satSelectMode == 3) ? settings.maxChannels : settings.numberOfChannels;
    if (activeChannels.size() > static_cast<size_t>(limit)) {
        activeChannels.resize(limit);
    }
    diagLog("PP: resize done limit=" + std::to_string(limit));

    std::vector<int> trackedPrns;
    for (const auto& ch : activeChannels) {
        trackedPrns.push_back(ch.PRN);
    }
    diagLog("PP: trackedPrns built count=" + std::to_string(trackedPrns.size()));

    // Notify the GUI thread so it can render the acquisition plot incrementally.
    emitStage1(results.acqResults, trackedPrns);
    diagLog("PP: emitStage1 done");

    if (activeChannels.empty()) {
        emitLog("[WARNING] No satellites passed the acquisition threshold.");
        return true;
    }

    emitLog("Acquisition completed. Channels allocated for tracking: " + std::to_string(activeChannels.size()) + "\n");

    std::ostringstream chanTable;
    chanTable << "  Channel Allocation Table:\n";
    for (size_t i = 0; i < activeChannels.size(); ++i) {
        chanTable << "   Ch " << (i + 1) << " -> PRN " << activeChannels[i].PRN
            << " (Doppler: " << (activeChannels[i].acquiredFreq - settings.IF) << " Hz, Code Phase: "
            << activeChannels[i].codePhase << ")\n";
    }
    emitLog(chanTable.str());

    // 4. Tracking Stage
    auto t_trk_start = std::chrono::high_resolution_clock::now();
    emitLog("\n   Tracking started...");

    std::ifstream fid(settings.fileName.toStdString(), std::ios::binary);
    if (!fid.is_open()) {
        emitLog("[ERROR] Failed to reopen file for tracking.");
        return false;
    }

    std::vector<ChannelTrackResult> rawTrackResults = Tracking::run(fid, activeChannels, settings);
    fid.close();

    auto t_trk_end = std::chrono::high_resolution_clock::now();
    double trkElapsed = std::chrono::duration<double>(t_trk_end - t_trk_start).count();

    results.trackResults.clear();
    for (const auto& tr : rawTrackResults) {
        if (tr.PRN > 0 && !tr.I_P.empty()) {
            results.trackResults.push_back(tr);
            double avgCNo = 0.0;
            for (double c : tr.CNo) avgCNo += c;
            avgCNo /= tr.CNo.empty() ? 1.0 : tr.CNo.size();

            emitLog("     > PRN " + std::to_string(tr.PRN) + " tracked successfully. Mean C/N0: " +
                std::to_string(static_cast<int>(avgCNo)) + " dB-Hz");
        }
    }

    std::ostringstream trkDone;
    trkDone << "   Tracking completed in " << std::fixed << std::setprecision(2) << trkElapsed << " s.\n";
    emitLog(trkDone.str());

    if (!results.trackResults.empty()) {
        emitStage2(results.trackResults);
    }

    // 5. Navigation Stage
    emitLog("   Calculating navigation solutions...");
    auto t_nav_start = std::chrono::high_resolution_clock::now();
    diagLog("PostProcessor: about to call PostNavigation::run, trackResults.size=" + std::to_string(results.trackResults.size()));

    if (results.trackResults.size() >= 4) {
        PostNavigation::run(results.trackResults, settings, results.navSolutions);
        emitStage3(results.navSolutions);

        auto t_nav_end = std::chrono::high_resolution_clock::now();
        double navElapsed = std::chrono::duration<double>(t_nav_end - t_nav_start).count();

        std::ostringstream navDone;
        navDone << "   Navigation position solution computed successfully in "
            << std::fixed << std::setprecision(2) << navElapsed << " s.\n";
        emitLog(navDone.str());
    }
    else {
        emitLog("[INFO] Tracked satellites (" + std::to_string(results.trackResults.size()) + " < 4). 3D Fix skipped.");
    }

    // -------------------------------------------------------------
    // Overall Elapsed Execution Summary
    // -------------------------------------------------------------
    auto endTime = std::chrono::high_resolution_clock::now();
    double totalElapsedSeconds = std::chrono::duration<double>(endTime - startTime).count();

    int minutes = static_cast<int>(totalElapsedSeconds) / 60;
    double seconds = totalElapsedSeconds - (minutes * 60);

    std::ostringstream endBanner;
    endBanner << "=============================================================\n"
        << " SDR Pipeline Execution Completed Successfully!\n"
        << " Total Elapsed Time: " << minutes << " min " << std::fixed << std::setprecision(2) << seconds << " s ("
        << totalElapsedSeconds << " seconds)\n"
        << "=============================================================\n";
    emitLog(endBanner.str());

    return true;
}