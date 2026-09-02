#pragma once

#include <QMainWindow>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QTextEdit>
#include <QProgressBar>
#include <memory>
#include <atomic>
#include "Settings.h"
#include "SDRPipelineWorker.h"

class QThread;
class SettingsWindow;
class SDRPlotWindow;

class SettingsWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit SettingsWindow(QWidget* parent = nullptr);
    ~SettingsWindow() override;

    void logMessage(const QString& msg);

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onBrowseFile();
    void onProbeDataClicked();
    void onRunSDRClicked();
    void onCancelSDRClicked();

    // Worker thread -> GUI thread handlers
    void onWorkerLog(const QString& msg);
    void onWorkerProgress(int percent);
    void onAcquisitionReady(const AcqResults& acqResults, const QVector<int>& trackedPrns);
    void onTrackingReady(const std::vector<ChannelTrackResult>& trackResults);
    void onNavigationReady(const NavSolutions& navSolutions);
    void onWorkerFinished(bool ok);
    void onThreadFinished();

private:
    void setupUI();
    void loadSettingsToUI();
    void saveUIToSettings();
    void startPipeline();
    void stopPipeline();

    // Returns the shared plot window, creating it on first use as an
    // independent parentless top-level window (so it does not minimize or
    // close together with this settings window).
    SDRPlotWindow* ensurePlotWindow();

    Settings settings;

    // Parameter UI Controls
    QLineEdit* editFileName{ nullptr };
    QPushButton* btnBrowse{ nullptr };
    QLineEdit* editSatelliteList{ nullptr };
    QComboBox* comboSatMode{ nullptr };
    QDoubleSpinBox* spinAcqThreshold{ nullptr };
    QSpinBox* spinChannels{ nullptr };
    QSpinBox* spinMsToProcess{ nullptr };
    QDoubleSpinBox* spinIF{ nullptr };
    QDoubleSpinBox* spinSamplingFreq{ nullptr };

    // Action Controls
    QPushButton* btnProbe{ nullptr };
    QPushButton* btnRun{ nullptr };
    QPushButton* btnCancel{ nullptr };
    QProgressBar* progressBar{ nullptr };
    QTextEdit* consoleOutput{ nullptr };

    // Pipeline worker thread (owned by this window)
    QThread* m_sdrThread{ nullptr };
    SDRPipelineWorker* m_worker{ nullptr };

    // Plot window lives on the GUI thread and is reused across runs.
    SDRPlotWindow* m_plotWindow{ nullptr };

    // Cached tracking results for sky plot fallback
    std::vector<ChannelTrackResult> m_lastTrackResults;

    // When true, the user requested to close the window while the worker was
    // still running. The actual close is deferred until the worker finishes so
    // the running QThread is never destroyed.
    bool m_closeRequested = false;
};
