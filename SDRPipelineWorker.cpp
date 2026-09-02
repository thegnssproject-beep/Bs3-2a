#include "SDRPipelineWorker.h"
#include <QString>
#include <fstream>
#include <cmath>

static void writeDiag(const std::string& line)
{
    std::ofstream out("C:/Yousuf_B2a/BS-3-2a_GUI/nav_diag.txt", std::ios::app);
    out << line << "\n";
}

SDRPipelineWorker::SDRPipelineWorker(const Settings& settings, QObject* parent)
    : QObject(parent), m_settings(settings)
{
}

void SDRPipelineWorker::process()
{
    {
        std::ofstream out("C:/Yousuf_B2a/BS-3-2a_GUI/nav_diag.txt", std::ios::trunc);
        out << "run started, msToProcess=" << m_settings.msToProcess << "\n";
    }

    auto logger = [this](const std::string& text) {
        if (text.find("3D Fix skipped") != std::string::npos ||
            text.find("computed successfully") != std::string::npos) {
            writeDiag("PIPELINE: " + text);
        }
        emit logMessage(QString::fromStdString(text));
        };

    auto onStage = [this](const PipelineStage& stage) {
        switch (stage.stage) {
        case 1:
            writeDiag("STAGE1 acquired PRNs:");
            for (int p : stage.trackedPrns) writeDiag("  " + std::to_string(p));
            emit progressChanged(30);
            emit acquisitionReady(
                stage.acqResults,
                QVector<int>(stage.trackedPrns.begin(), stage.trackedPrns.end()));
            break;
        case 2:
            {
                int ntrk = 0;
                for (const auto& tr : stage.trackResults)
                    if (tr.PRN > 0 && !tr.I_P.empty()) ntrk++;
                writeDiag("STAGE2 tracked channels=" + std::to_string(ntrk) +
                    " total=" + std::to_string(stage.trackResults.size()));
                for (const auto& tr : stage.trackResults)
                    if (tr.PRN > 0)
                        writeDiag("  CH PRN=" + std::to_string(tr.PRN) +
                            " I_P.size=" + std::to_string(tr.I_P.size()));
                emit progressChanged(85);
                emit trackingReady(stage.trackResults);
            }
            break;
        case 3:
            {
                const auto& ns = stage.navSolutions;
                int nfix = 0;
                double maxEl = 0.0;
                for (size_t i = 0; i < ns.latitude.size(); ++i)
                    if (std::abs(ns.latitude[i]) > 1e-4) nfix++;
                for (const auto& elRow : ns.elevation)
                    for (double e : elRow) if (e > maxEl) maxEl = e;
                writeDiag("STAGE3 nav lat.size=" + std::to_string(ns.latitude.size()) +
                    " fixes=" + std::to_string(nfix) +
                    " activePrns=" + std::to_string(ns.activePrns.size()) +
                    " maxEl=" + std::to_string(maxEl));
                emit progressChanged(98);
                emit navigationReady(stage.navSolutions);
            }
            break;
        default:
            break;
        }
        };

    emit progressChanged(2);

    bool ok = PostProcessor::runPipeline(m_settings, m_results, logger, onStage);
    writeDiag("run finished ok=" + std::string(ok ? "true" : "false"));

    emit progressChanged(100);
    emit finished(ok);
}
