#pragma once

#include <QObject>
#include <QVector>
#include <vector>
#include "Settings.h"
#include "Acquisition.h"
#include "Tracking.h"
#include "PostNavigation.h"
#include "PostProcessor.h"

// Register the custom payload types so they can travel across queued
// (cross-thread) signal connections.
Q_DECLARE_METATYPE(AcqResults)
Q_DECLARE_METATYPE(std::vector<ChannelTrackResult>)
Q_DECLARE_METATYPE(NavSolutions)

// Runs the SDR post-processing pipeline on a dedicated worker thread so the
// GUI stays responsive while acquisition / tracking / navigation run.
// All signals are emitted from the worker thread and are queued to the GUI
// thread via the standard Qt connection mechanism.
class SDRPipelineWorker : public QObject
{
    Q_OBJECT

public:
    explicit SDRPipelineWorker(const Settings& settings, QObject* parent = nullptr);

public slots:
    void process();

signals:
    void logMessage(const QString& msg);
    void progressChanged(int percent);
    void acquisitionReady(const AcqResults& acqResults, const QVector<int>& trackedPrns);
    void trackingReady(const std::vector<ChannelTrackResult>& trackResults);
    void navigationReady(const NavSolutions& navSolutions);
    void finished(bool ok);

private:
    Settings m_settings;
    SDRPipelineResults m_results;
};
