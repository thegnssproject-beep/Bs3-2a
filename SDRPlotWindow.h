#pragma once

#include <QMainWindow>
#include <QTabWidget>
#include <QVBoxLayout>
#include <vector>
#include "Acquisition.h"
#include "Tracking.h"
#include "PostNavigation.h"

#if __has_include("qcustomplot.h")
#include "qcustomplot.h"
#endif

class SDRPlotWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit SDRPlotWindow(QWidget* parent = nullptr);

    void plotAcquisitionResults(const AcqResults& acq);
    void plotTrackingResults(const ChannelTrackResult& trackRes);
    void plotNavigationResults(const NavSolutions& nav);

private:
    void setupUI();

    QTabWidget* tabWidget;

#if __has_include("qcustomplot.h")
    QCustomPlot* acqPlot;
    QCustomPlot* trackingPlot;
    QCustomPlot* cnoPlot;
    QCustomPlot* navPlot;
    QCustomPlot* skyPlot;
#endif
};