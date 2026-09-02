#pragma once

#include <QMainWindow>
#include <QTabWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <qcustomplot.h>
#include <vector>
#include "Acquisition.h"
#include "Tracking.h"
#include "PostNavigation.h"
#include "IQScatterPlotWidget.h"

class SDRPlotWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit SDRPlotWindow(QWidget* parent = nullptr);
    ~SDRPlotWindow() override = default;

    void plotAcquisitionResults(const AcqResults& acqRes, double threshold, const std::vector<int>& trackedPrns, double ifFreq);
    void plotTrackingResults(const std::vector<ChannelTrackResult>& trackResults);
    void plotNavigationResults(const NavSolutions& navSol, const std::vector<ChannelTrackResult>* trackResults = nullptr);

    // Populates this (already-independent) window with a single channel's
    // tracking view and brings it to the front. Used to open comparison
    // popups per PRN from the acquisition table's View buttons.
    void showTrackingChannel(const std::vector<ChannelTrackResult>& trackResults, size_t index);

public slots:
    void onChannelViewButtonClicked(int channelIndex);

private slots:
    void onPrevSatelliteClicked();
    void onNextSatelliteClicked();
    void updateTrackingChannelView(size_t index);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void setupUI();
    void setupAcquisitionTab();
    void setupTrackingTab();
    void setupCNoTab();
    void setupNavigationTab();
    void setupSkyPlotTab();
    void drawSkyPlotBaseGrid();

    // UI Root Container
    QTabWidget* m_tabWidget = nullptr;

    // Tab 1: Acquisition
    QCustomPlot* m_acqPlot = nullptr;
    QTableWidget* m_channelTable = nullptr;

    // Tab 2: Tracking Subplots (MATLAB 7-Panel Layout)
    IQScatterPlotWidget* m_scatterPlotWidget = nullptr;
    QCustomPlot* m_plotNavBits = nullptr;
    QCustomPlot* m_plotRawPLL = nullptr;
    QCustomPlot* m_plotCorrResults = nullptr;
    QCustomPlot* m_plotFiltPLL = nullptr;   // <-- Declared here
    QCustomPlot* m_plotRawDLL = nullptr;
    QCustomPlot* m_plotFiltDLL = nullptr;

    QPushButton* m_btnPrevSat = nullptr;
    QPushButton* m_btnNextSat = nullptr;
    QLabel* m_lblSatIndex = nullptr;

    // Tab 3: C/N0 Quality
    QCustomPlot* m_cnoPlot = nullptr;

    // Tab 4: Navigation Track
    QCustomPlot* m_navTrackPlot = nullptr;

    // Tab 5: Sky View Polar Plot
    QCustomPlot* m_skyPlot = nullptr;

    // Data Cache
    std::vector<ChannelTrackResult> m_trackResults;
    size_t m_currentTrackIndex = 0;
};