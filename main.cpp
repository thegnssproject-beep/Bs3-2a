#include <QApplication>
#include <QString>
#include <fstream>
#include <iostream>
#include <cmath>
#include <exception>
#include "SettingsWindow.h"
#include "PostProcessor.h"

static void runNavDiagnostic()
{
    std::ofstream out("C:/Yousuf_B2a/BS-3-2a_GUI/nav_diag.txt", std::ios::trunc);
    out << "diag started\n";
    out.flush();

    Settings s;
    s.fileName = "C:/Yousuf_B2a/dump1_ch3_1.bin";
    s.fileType = 1;
    s.IF = 13.55e6;
    s.samplingFreq = 99.375e6;
    s.codeLength = 10230;
    s.codeFreqBasis = 10.23e6;
    s.fineNoncoh = 15;
    s.msToProcess = 30000;
    s.numberOfChannels = 12;
    s.acqThreshold = 1.0;
    s.targetSatList = "19 20 30 37 40 43";
    s.acqSatelliteList = "19 20 30 37 40 43";
    s.CNoInterval = 200;
    s.navSolPeriod = 500;
    s.elevationMask = 5;
    s.pilotTRKflag = true;
    s.dllNoiseBandwidth = 2.0;
    s.pllNoiseBandwidth = 20.0;
    s.intTime = 0.001;
    s.startOffset = 0.068802;

    SDRPipelineResults results;
    int stageReached = 0;
    int nTracked = 0;
    std::vector<int> acqPrns;

    auto writeLog = [&](const char* line) {
        out << line << "\n";
        std::cout << line << "\n";
        };

    auto logger = [&](const std::string& text) {
        if (text.find("3D Fix skipped") != std::string::npos ||
            text.find("computed successfully") != std::string::npos) {
            writeLog(("PIPELINE: " + text).c_str());
        }
        };

    auto onStage = [&](const PipelineStage& st) {
        if (st.stage == 1) {
            acqPrns = st.trackedPrns;
            stageReached = 1;
        }
        else if (st.stage == 2) {
            stageReached = 2;
            nTracked = 0;
            for (const auto& tr : st.trackResults)
                if (tr.PRN > 0 && !tr.I_P.empty()) nTracked++;
        }
        else if (st.stage == 3) {
            stageReached = 3;
            const auto& ns = st.navSolutions;
            int nfix = 0;
            double maxEl = 0.0;
            for (size_t i = 0; i < ns.latitude.size(); ++i)
                if (std::abs(ns.latitude[i]) > 1e-4) nfix++;
            for (const auto& elRow : ns.elevation)
                for (double e : elRow) if (e > maxEl) maxEl = e;

            writeLog("[NAV] lat.size=");
            out << ns.latitude.size() << " ";
            std::cout << ns.latitude.size() << " ";
            writeLog("[NAV] fixes=");
            out << nfix << " ";
            std::cout << nfix << " ";
            writeLog("[NAV] activePrns=");
            out << ns.activePrns.size() << " ";
            std::cout << ns.activePrns.size() << " ";
            writeLog("[NAV] maxEl=");
            out << maxEl << "\n";
            std::cout << maxEl << "\n";
        }
        };

    out << "entering runPipeline\n";
    out.flush();

    bool ok = false;
    try {
        ok = PostProcessor::runPipeline(s, results, logger, onStage);
        out << "runPipeline returned\n";
    }
    catch (const std::exception& e) {
        out << "EXCEPTION: " << e.what() << "\n";
    }
    catch (...) {
        out << "EXCEPTION: unknown\n";
    }
    out.flush();

    writeLog("[RESULT] ok=");
    out << ok << "\n";
    std::cout << ok << "\n";

    writeLog("[RESULT] stageReached=");
    out << stageReached << "\n";
    std::cout << stageReached << "\n";

    writeLog("[RESULT] acqPrns=");
    for (int p : acqPrns) { out << p << " "; std::cout << p << " "; }
    writeLog("");
    writeLog("[RESULT] nTracked=");
    out << nTracked << "\n";
    std::cout << nTracked << "\n";

    if (stageReached >= 2) {
        writeLog("[RESULT] trackResults.size=");
        out << results.trackResults.size() << "\n";
        std::cout << results.trackResults.size() << "\n";
        for (const auto& tr : results.trackResults) {
            if (tr.PRN > 0) {
                writeLog(("[CH] PRN=" + std::to_string(tr.PRN) + " I_P.size=" +
                    std::to_string(tr.I_P.size())).c_str());
            }
        }
    }

    out.close();
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    if (argc > 1 && QString(argv[1]) == "--nav-diag") {
        runNavDiagnostic();
        return 0;
    }

    SettingsWindow window;
    window.show();

    return app.exec();
}
