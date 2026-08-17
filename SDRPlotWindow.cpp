#include "SDRPlotWindow.h"
#include "SkyPlot.h"
#include <cmath>

SDRPlotWindow::SDRPlotWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setupUI();
    setWindowTitle("BDS-3 B2a SDR Analysis & Plotter");
    resize(1050, 720);
}

void SDRPlotWindow::setupUI()
{
    tabWidget = new QTabWidget(this);
    setCentralWidget(tabWidget);

#if __has_include("qcustomplot.h")
    acqPlot = new QCustomPlot(this);
    trackingPlot = new QCustomPlot(this);
    cnoPlot = new QCustomPlot(this);
    navPlot = new QCustomPlot(this);
    skyPlot = new QCustomPlot(this);

    tabWidget->addTab(acqPlot, "Acquisition Peaks");
    tabWidget->addTab(trackingPlot, "Tracking (DLL/PLL)");
    tabWidget->addTab(cnoPlot, "C/N0 Signal Quality");
    tabWidget->addTab(navPlot, "Navigation Position Track");
    tabWidget->addTab(skyPlot, "Sky View Polar Plot");
#endif
}

void SDRPlotWindow::plotAcquisitionResults(const AcqResults& acq)
{
#if __has_include("qcustomplot.h")
    acqPlot->clearGraphs();
    acqPlot->clearPlottables();

    QVector<double> prns, acquiredMetrics, notAcquiredMetrics;
    const double threshold = 1.1;

    for (size_t prn = 1; prn < acq.peakMetric.size(); ++prn) {
        if (acq.peakMetric[prn] > 0.0) {
            prns.append(static_cast<double>(prn));
            if (acq.peakMetric[prn] >= threshold) {
                acquiredMetrics.append(acq.peakMetric[prn]);
                notAcquiredMetrics.append(0.0);
            }
            else {
                acquiredMetrics.append(0.0);
                notAcquiredMetrics.append(acq.peakMetric[prn]);
            }
        }
    }

    if (!prns.isEmpty()) {
        QCPBars* barNotAcq = new QCPBars(acqPlot->xAxis, acqPlot->yAxis);
        barNotAcq->setData(prns, notAcquiredMetrics);
        barNotAcq->setWidth(0.7);
        barNotAcq->setBrush(QColor(50, 40, 160));
        barNotAcq->setPen(QPen(Qt::black));
        barNotAcq->setName("Not acquired signals");

        QCPBars* barAcq = new QCPBars(acqPlot->xAxis, acqPlot->yAxis);
        barAcq->setData(prns, acquiredMetrics);
        barAcq->setWidth(0.7);
        barAcq->setBrush(QColor(40, 180, 40));
        barAcq->setPen(QPen(Qt::black));
        barAcq->setName("Acquired signals");

        acqPlot->xAxis->setLabel("PRN number (no bar - SV is not in the acquisition list)");
        acqPlot->yAxis->setLabel("Acquisition Metric");
        acqPlot->xAxis->setRange(0, 25);
        acqPlot->yAxis->setRange(0, 1.5);
        acqPlot->legend->setVisible(true);
        acqPlot->replot();
    }
#endif
}

void SDRPlotWindow::plotTrackingResults(const ChannelTrackResult& trackRes)
{
#if __has_include("qcustomplot.h")
    trackingPlot->clearGraphs();

    QVector<double> timeMs(trackRes.I_P.size()), IP(trackRes.I_P.size()), QP(trackRes.Q_P.size());
    for (size_t i = 0; i < trackRes.I_P.size(); ++i) {
        timeMs[i] = static_cast<double>(i);
        IP[i] = trackRes.I_P[i];
        QP[i] = trackRes.Q_P[i];
    }

    trackingPlot->addGraph();
    trackingPlot->graph(0)->setData(timeMs, IP);
    trackingPlot->graph(0)->setPen(QPen(Qt::blue));
    trackingPlot->graph(0)->setName("In-Phase Prompt (I_P)");

    trackingPlot->addGraph();
    trackingPlot->graph(1)->setData(timeMs, QP);
    trackingPlot->graph(1)->setPen(QPen(Qt::red));
    trackingPlot->graph(1)->setName("Quadrature Prompt (Q_P)");

    trackingPlot->xAxis->setLabel("Time (ms)");
    trackingPlot->yAxis->setLabel("Correlator Output Amplitude");
    trackingPlot->legend->setVisible(true);

    trackingPlot->rescaleAxes();
    trackingPlot->replot();

    // Carrier-to-Noise Ratio (C/N0) Plot
    if (!trackRes.CNo.empty()) {
        cnoPlot->clearGraphs();
        QVector<double> cnoX(trackRes.CNo.size()), cnoY(trackRes.CNo.size());
        for (size_t i = 0; i < trackRes.CNo.size(); ++i) {
            cnoX[i] = static_cast<double>(i);
            cnoY[i] = trackRes.CNo[i];
        }

        cnoPlot->addGraph();
        cnoPlot->graph(0)->setData(cnoX, cnoY);
        cnoPlot->graph(0)->setPen(QPen(Qt::darkGreen, 2));
        cnoPlot->xAxis->setLabel("Epoch / Interval");
        cnoPlot->yAxis->setLabel("Carrier-to-Noise Ratio (dB-Hz)");

        if (cnoPlot->plotLayout()->rowCount() == 1) {
            cnoPlot->plotLayout()->insertRow(0);
            cnoPlot->plotLayout()->addElement(0, 0, new QCPTextElement(cnoPlot, "Tracking C/N0 Estimation", QFont("sans", 10, QFont::Bold)));
        }

        cnoPlot->rescaleAxes();
        cnoPlot->replot();
    }
#endif
}

void SDRPlotWindow::plotNavigationResults(const NavSolutions& nav)
{
#if __has_include("qcustomplot.h")
    navPlot->clearGraphs();

    QVector<double> east, north;
    for (size_t i = 0; i < nav.latitude.size(); ++i) {
        if (!std::isnan(nav.latitude[i]) && !std::isnan(nav.longitude[i])) {
            east.append(nav.longitude[i]);
            north.append(nav.latitude[i]);
        }
    }

    navPlot->addGraph();
    navPlot->graph(0)->setData(east, north);
    navPlot->graph(0)->setPen(QPen(Qt::darkGreen, 2));
    navPlot->graph(0)->setLineStyle(QCPGraph::lsLine);
    navPlot->graph(0)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, 5));

    navPlot->xAxis->setLabel("Longitude (Degrees)");
    navPlot->yAxis->setLabel("Latitude (Degrees)");

    if (navPlot->plotLayout()->rowCount() == 1) {
        navPlot->plotLayout()->insertRow(0);
        navPlot->plotLayout()->addElement(0, 0, new QCPTextElement(navPlot, "Receiver WGS-84 Position Estimate", QFont("sans", 10, QFont::Bold)));
    }

    navPlot->rescaleAxes();
    navPlot->replot();

    // Render Polar Sky View Map
    SkyPlot::draw(skyPlot, nav.azimuth, nav.elevation, nav.activePrns);
#endif
}