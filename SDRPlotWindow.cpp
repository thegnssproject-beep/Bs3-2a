#define _USE_MATH_DEFINES
#include <cmath>
#include "SDRPlotWindow.h"
#include <QHeaderView>
#include <QResizeEvent>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

SDRPlotWindow::SDRPlotWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("BDS-3 B2a SDR Analysis & Plotter");
    resize(1200, 780);
    setupUI();
}

void SDRPlotWindow::setupUI()
{
    m_tabWidget = new QTabWidget(this);
    setCentralWidget(m_tabWidget);

    setupAcquisitionTab();
    setupTrackingTab();
    setupCNoTab();
    setupNavigationTab();
    setupSkyPlotTab();
}

void SDRPlotWindow::setupAcquisitionTab()
{
    QWidget* tab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(tab);

    m_acqPlot = new QCustomPlot(tab);
    m_acqPlot->setMinimumHeight(340);
    m_acqPlot->xAxis->setLabel("PRN Satellite Number");
    m_acqPlot->yAxis->setLabel("Acquisition Metric");
    m_acqPlot->xAxis->setRange(16, 64);
    m_acqPlot->yAxis->setRange(0, 3.0);
    m_acqPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    m_acqPlot->setNoAntialiasingOnDrag(true);

    QLabel* lblStatus = new QLabel("Channel Status", tab);
    lblStatus->setAlignment(Qt::AlignCenter);
    QFont f = lblStatus->font();
    f.setBold(true);
    f.setPointSize(11);
    lblStatus->setFont(f);

    m_channelTable = new QTableWidget(tab);
    m_channelTable->setColumnCount(7);
    m_channelTable->setHorizontalHeaderLabels({ "Channel", "PRN", "Carrier Frequency", "Doppler", "Code Offset", "Status", "Graphs" });
    m_channelTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_channelTable->verticalHeader()->setVisible(false);
    m_channelTable->setFixedHeight(220);

    layout->addWidget(m_acqPlot);
    layout->addWidget(lblStatus);
    layout->addWidget(m_channelTable);

    m_tabWidget->addTab(tab, "Acquisition Peaks");
}

void SDRPlotWindow::setupTrackingTab()
{
    QWidget* tab = new QWidget();
    QVBoxLayout* mainLayout = new QVBoxLayout(tab);

    QHBoxLayout* navLayout = new QHBoxLayout();
    m_btnPrevSat = new QPushButton("<< Previous Satellite", tab);
    m_btnNextSat = new QPushButton("Next Satellite >>", tab);
    m_lblSatIndex = new QLabel("Displaying PRN -- (Satellite 0 of 0)", tab);
    m_lblSatIndex->setAlignment(Qt::AlignCenter);
    QFont f = m_lblSatIndex->font();
    f.setBold(true);
    m_lblSatIndex->setFont(f);

    navLayout->addWidget(m_btnPrevSat);
    navLayout->addWidget(m_lblSatIndex);
    navLayout->addWidget(m_btnNextSat);
    mainLayout->addLayout(navLayout);

    QGridLayout* grid = new QGridLayout();

    auto configPlot = [](QCustomPlot* p, const QString& title, const QString& yLabel = "Amplitude") {
        p->plotLayout()->insertRow(0);
        p->plotLayout()->addElement(0, 0, new QCPTextElement(p, title, QFont("Arial", 9, QFont::Bold)));
        p->xAxis->setLabel("Time (s)");
        p->yAxis->setLabel(yLabel);
        p->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
        p->setNoAntialiasingOnDrag(true);
        p->setPlottingHint(QCP::phFastPolylines, true);
        };

    // Row 0: Scatter Plot (Col 0) + Navigation Bits (Cols 1 & 2)
    m_scatterPlotWidget = new IQScatterPlotWidget(tab);
    grid->addWidget(m_scatterPlotWidget, 0, 0);

    m_plotNavBits = new QCustomPlot(tab);
    configPlot(m_plotNavBits, "Bits of the navigation message", "");
    m_plotNavBits->addGraph();
    m_plotNavBits->graph(0)->setPen(QPen(QColor(0, 114, 189), 1.0));
    grid->addWidget(m_plotNavBits, 0, 1, 1, 2);

    // Row 1: Raw PLL (Col 0) + Correlation Results (Cols 1 & 2)
    m_plotRawPLL = new QCustomPlot(tab);
    configPlot(m_plotRawPLL, "Raw PLL discriminator");
    m_plotRawPLL->addGraph();
    m_plotRawPLL->graph(0)->setPen(QPen(Qt::red, 1.0));
    grid->addWidget(m_plotRawPLL, 1, 0);

    m_plotCorrResults = new QCustomPlot(tab);
    configPlot(m_plotCorrResults, "Correlation results", "");
    m_plotCorrResults->addGraph();
    m_plotCorrResults->graph(0)->setPen(QPen(QColor(0, 114, 189), 1.0));
    m_plotCorrResults->graph(0)->setName("Early");
    m_plotCorrResults->addGraph();
    m_plotCorrResults->graph(1)->setPen(QPen(QColor(217, 83, 25), 1.0));
    m_plotCorrResults->graph(1)->setName("Prompt");
    m_plotCorrResults->addGraph();
    m_plotCorrResults->graph(2)->setPen(QPen(QColor(237, 177, 32), 1.0));
    m_plotCorrResults->graph(2)->setName("Late");
    grid->addWidget(m_plotCorrResults, 1, 1, 1, 2);

    // Row 2: Filtered PLL (Col 0) + Raw DLL (Col 1) + Filtered DLL (Col 2)
    m_plotFiltPLL = new QCustomPlot(tab);
    configPlot(m_plotFiltPLL, "Filtered PLL discriminator");
    m_plotFiltPLL->addGraph();
    m_plotFiltPLL->graph(0)->setPen(QPen(Qt::blue, 1.0));
    grid->addWidget(m_plotFiltPLL, 2, 0);

    m_plotRawDLL = new QCustomPlot(tab);
    configPlot(m_plotRawDLL, "Raw DLL discriminator");
    m_plotRawDLL->addGraph();
    m_plotRawDLL->graph(0)->setPen(QPen(Qt::red, 1.0));
    grid->addWidget(m_plotRawDLL, 2, 1);

    m_plotFiltDLL = new QCustomPlot(tab);
    configPlot(m_plotFiltDLL, "Filtered DLL discriminator");
    m_plotFiltDLL->addGraph();
    m_plotFiltDLL->graph(0)->setPen(QPen(Qt::blue, 1.0));
    grid->addWidget(m_plotFiltDLL, 2, 2);

    mainLayout->addLayout(grid);

    connect(m_btnPrevSat, &QPushButton::clicked, this, &SDRPlotWindow::onPrevSatelliteClicked);
    connect(m_btnNextSat, &QPushButton::clicked, this, &SDRPlotWindow::onNextSatelliteClicked);

    m_tabWidget->addTab(tab, "Tracking (DLL/PLL)");
}

void SDRPlotWindow::setupCNoTab()
{
    QWidget* tab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(tab);

    m_cnoPlot = new QCustomPlot(tab);
    m_cnoPlot->xAxis->setLabel("Tracking Time (s)");
    m_cnoPlot->yAxis->setLabel("C/N0 (dB-Hz)");
    m_cnoPlot->legend->setVisible(true);
    m_cnoPlot->yAxis->setRange(20, 55);

    m_cnoPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    m_cnoPlot->setNoAntialiasingOnDrag(true);
    m_cnoPlot->setPlottingHint(QCP::phFastPolylines, true);

    layout->addWidget(m_cnoPlot);
    m_tabWidget->addTab(tab, "C/N0 Signal Quality");
}

void SDRPlotWindow::setupNavigationTab()
{
    QWidget* tab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(tab);

    m_navTrackPlot = new QCustomPlot(tab);
    m_navTrackPlot->xAxis->setLabel("Longitude (Degrees)");
    m_navTrackPlot->yAxis->setLabel("Latitude (Degrees)");
    m_navTrackPlot->addGraph();
    m_navTrackPlot->graph(0)->setPen(QPen(QColor(0, 114, 189), 2.0));
    m_navTrackPlot->graph(0)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 5));
    m_navTrackPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);

    layout->addWidget(m_navTrackPlot);
    m_tabWidget->addTab(tab, "Navigation Position Track");
}

void SDRPlotWindow::setupSkyPlotTab()
{
    QWidget* tab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(tab);

    m_skyPlot = new QCustomPlot(tab);
    m_skyPlot->xAxis->setVisible(false);
    m_skyPlot->yAxis->setVisible(false);
    m_skyPlot->xAxis->setRange(-1.15, 1.15);
    m_skyPlot->yAxis->setRange(-1.15, 1.15);
    m_skyPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);

    layout->addWidget(m_skyPlot);
    m_tabWidget->addTab(tab, "Sky View Polar Plot");
    drawSkyPlotBaseGrid();
}

void SDRPlotWindow::drawSkyPlotBaseGrid()
{
    for (double el : {0.0, 30.0, 60.0}) {
        double r = (90.0 - el) / 90.0;
        QCPItemEllipse* circle = new QCPItemEllipse(m_skyPlot);
        circle->topLeft->setCoords(-r, r);
        circle->bottomRight->setCoords(r, -r);
        circle->setPen(QPen(QColor(180, 180, 180), 1.0, Qt::DashLine));

        if (el > 0.0) {
            QCPItemText* elText = new QCPItemText(m_skyPlot);
            elText->position->setCoords(0.02, r);
            elText->setText(QString::number(static_cast<int>(el)) + "°");
            elText->setFont(QFont("Arial", 8));
            elText->setColor(QColor(130, 130, 130));
        }
    }

    for (int deg = 0; deg < 360; deg += 30) {
        double rad = deg * (M_PI / 180.0);
        double x = std::sin(rad);
        double y = std::cos(rad);

        QCPItemLine* spoke = new QCPItemLine(m_skyPlot);
        spoke->start->setCoords(0, 0);
        spoke->end->setCoords(x, y);
        spoke->setPen(QPen(QColor(215, 215, 215), 1.0, Qt::DotLine));

        QCPItemText* degLabel = new QCPItemText(m_skyPlot);
        degLabel->position->setCoords(x * 1.07, y * 1.07);
        degLabel->setText(QString::number(deg) + "°");
        degLabel->setFont(QFont("Arial", 8, QFont::Bold));
        degLabel->setColor(Qt::black);
    }
}

void SDRPlotWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    if (m_skyPlot) {
        m_skyPlot->xAxis->setRange(-1.15, 1.15);
        m_skyPlot->yAxis->setRange(-1.15, 1.15);
        m_skyPlot->replot();
    }
}

void SDRPlotWindow::plotAcquisitionResults(const AcqResults& acqRes, double threshold, const std::vector<int>& trackedPrns, double ifFreq)
{
    m_acqPlot->clearPlottables();
    m_acqPlot->clearItems();

    QCPBars* barsUnacq = new QCPBars(m_acqPlot->xAxis, m_acqPlot->yAxis);
    barsUnacq->setName("Not Acquired (< 1.50)");
    barsUnacq->setBrush(QColor(91, 155, 213));
    barsUnacq->setPen(Qt::NoPen);

    QCPBars* barsAcq = new QCPBars(m_acqPlot->xAxis, m_acqPlot->yAxis);
    barsAcq->setName("Selected & Tracked");
    barsAcq->setBrush(QColor(237, 125, 49));
    barsAcq->setPen(Qt::NoPen);

    QVector<double> ticksUnacq, valsUnacq;
    QVector<double> ticksAcq, valsAcq;

    for (int p = 19; p <= 63; ++p) {
        double metric = (p < static_cast<int>(acqRes.peakMetric.size())) ? acqRes.peakMetric[p] : 0.0;
        bool isTracked = (std::find(trackedPrns.begin(), trackedPrns.end(), p) != trackedPrns.end());

        if (isTracked && metric >= threshold) {
            ticksAcq.push_back(p);
            valsAcq.push_back(metric);
        }
        else {
            ticksUnacq.push_back(p);
            valsUnacq.push_back(metric);
        }
    }

    barsUnacq->setData(ticksUnacq, valsUnacq);
    barsAcq->setData(ticksAcq, valsAcq);

    QCPItemStraightLine* threshLine = new QCPItemStraightLine(m_acqPlot);
    threshLine->point1->setCoords(16, threshold);
    threshLine->point2->setCoords(64, threshold);
    threshLine->setPen(QPen(Qt::red, 1.5, Qt::DashLine));

    m_acqPlot->legend->setVisible(true);
    m_acqPlot->replot();

    m_channelTable->setRowCount(static_cast<int>(trackedPrns.size()));
    for (size_t i = 0; i < trackedPrns.size(); ++i) {
        int prn = trackedPrns[i];
        double fCarr = acqRes.carrFreq[prn];
        double doppler = fCarr - ifFreq;
        double codeOffset = acqRes.codePhase[prn];

        m_channelTable->setItem(static_cast<int>(i), 0, new QTableWidgetItem(QString::number(i + 1)));
        m_channelTable->setItem(static_cast<int>(i), 1, new QTableWidgetItem(QString::number(prn)));
        m_channelTable->setItem(static_cast<int>(i), 2, new QTableWidgetItem(QString::number(fCarr, 'e', 4)));
        m_channelTable->setItem(static_cast<int>(i), 3, new QTableWidgetItem(QString::number(doppler, 'f', 0)));
        m_channelTable->setItem(static_cast<int>(i), 4, new QTableWidgetItem(QString::number(codeOffset, 'f', 0)));
        m_channelTable->setItem(static_cast<int>(i), 5, new QTableWidgetItem("T"));

        QPushButton* btnView = new QPushButton("View");
        btnView->setStyleSheet("QPushButton { background-color: #0078D7; color: white; font-weight: bold; border-radius: 3px; padding: 2px 8px; } QPushButton:hover { background-color: #1E90FF; }");
        connect(btnView, &QPushButton::clicked, this, [this, prn]() {
            this->onChannelViewButtonClicked(prn);
            });
        m_channelTable->setCellWidget(static_cast<int>(i), 6, btnView);
    }
}

void SDRPlotWindow::onChannelViewButtonClicked(int prn)
{
    // The acquisition table's row PRN may not line up positionally with the
    // tracking results (inactive/empty channels are dropped upstream), so
    // locate the channel by matching PRN instead of using the row index.
    size_t matchIdx = 0;
    bool found = false;
    for (size_t i = 0; i < m_trackResults.size(); ++i) {
        if (m_trackResults[i].PRN == prn) { matchIdx = i; found = true; break; }
    }

    if (found) {
        // Open an independent popup window for this PRN so tracking views of
        // two or more satellites can be compared side by side.
        SDRPlotWindow* popup = new SDRPlotWindow();
        popup->setAttribute(Qt::WA_DeleteOnClose);
        popup->showTrackingChannel(m_trackResults, matchIdx);
    }
    else if (m_tabWidget) {
        // Tracking results are not ready yet; fall back to the main window's
        // tracking tab so the user still ends up somewhere useful.
        m_tabWidget->setCurrentIndex(1);
    }
}

void SDRPlotWindow::showTrackingChannel(const std::vector<ChannelTrackResult>& trackResults, size_t index)
{
    m_trackResults = trackResults;
    if (m_trackResults.empty()) return;
    if (index >= m_trackResults.size()) index = m_trackResults.size() - 1;

    m_tabWidget->setCurrentIndex(1);
    setWindowTitle(QString("PRN %1 - Tracking").arg(m_trackResults[index].PRN));
    updateTrackingChannelView(index);
    show();
    raise();
    activateWindow();
}

void SDRPlotWindow::plotTrackingResults(const std::vector<ChannelTrackResult>& trackResults)
{
    m_trackResults = trackResults;
    m_currentTrackIndex = 0;

    if (!m_trackResults.empty()) {
        updateTrackingChannelView(0);
    }

    m_cnoPlot->clearGraphs();
    const QColor colors[7] = {
        QColor(0, 114, 189), QColor(217, 83, 25), QColor(237, 177, 32),
        QColor(126, 47, 142), QColor(119, 172, 48), QColor(77, 190, 238), QColor(162, 20, 47)
    };

    const int decimationStep = 200;

    for (size_t i = 0; i < m_trackResults.size(); ++i) {
        const auto& tr = m_trackResults[i];
        if (tr.CNo.empty()) continue;

        QCPGraph* g = m_cnoPlot->addGraph();
        g->setName(QString("PRN %1").arg(tr.PRN));
        g->setPen(QPen(colors[i % 7], 2.0));

        int rawCount = static_cast<int>(tr.CNo.size());
        int downsampledCount = rawCount / decimationStep;
        if (downsampledCount == 0) downsampledCount = 1;

        QVector<double> x(downsampledCount), y(downsampledCount);
        double smoothVal = tr.CNo[0];

        for (int k = 0; k < downsampledCount; ++k) {
            int startIdx = k * decimationStep;
            int endIdx = std::min(startIdx + decimationStep, rawCount);

            double sumVal = 0.0;
            for (int j = startIdx; j < endIdx; ++j) {
                sumVal += tr.CNo[j];
            }
            double blockAvg = sumVal / (endIdx - startIdx);
            smoothVal = 0.5 * blockAvg + 0.5 * smoothVal;

            x[k] = (startIdx + (endIdx - startIdx) / 2) * 0.001;
            y[k] = smoothVal;
        }

        g->setData(x, y, true);
    }

    m_cnoPlot->xAxis->rescale();
    m_cnoPlot->replot(QCustomPlot::rpQueuedReplot);
}

void SDRPlotWindow::updateTrackingChannelView(size_t index)
{
    if (index >= m_trackResults.size()) return;
    m_currentTrackIndex = index;

    const auto& tr = m_trackResults[index];
    m_lblSatIndex->setText(QString("Displaying PRN %1 (Satellite %2 of %3)")
        .arg(tr.PRN)
        .arg(index + 1)
        .arg(m_trackResults.size()));

    m_scatterPlotWidget->setChannelData(tr);

    int count = static_cast<int>(tr.I_P.size());
    QVector<double> t(count), iPrompt(count), rawPll(count), filtPll(count), rawDll(count), filtDll(count);
    QVector<double> envE(count), envP(count), envL(count);

    for (int k = 0; k < count; ++k) {
        t[k] = k * 0.001;
        iPrompt[k] = tr.I_P[k];
        rawPll[k] = tr.pllError[k];
        filtPll[k] = tr.pllDiscrFilt[k];
        rawDll[k] = tr.dllError[k];
        filtDll[k] = tr.dllDiscrFilt[k];

        envE[k] = std::sqrt(tr.I_E[k] * tr.I_E[k] + tr.Q_E[k] * tr.Q_E[k]);
        envP[k] = std::sqrt(tr.I_P[k] * tr.I_P[k] + tr.Q_P[k] * tr.Q_P[k]);
        envL[k] = std::sqrt(tr.I_L[k] * tr.I_L[k] + tr.Q_L[k] * tr.Q_L[k]);
    }

    m_plotNavBits->graph(0)->setData(t, iPrompt, true);
    m_plotNavBits->rescaleAxes();
    m_plotNavBits->replot(QCustomPlot::rpQueuedReplot);

    m_plotRawPLL->graph(0)->setData(t, rawPll, true);
    m_plotRawPLL->rescaleAxes();
    m_plotRawPLL->replot(QCustomPlot::rpQueuedReplot);

    m_plotCorrResults->graph(0)->setData(t, envE, true);
    m_plotCorrResults->graph(1)->setData(t, envP, true);
    m_plotCorrResults->graph(2)->setData(t, envL, true);
    m_plotCorrResults->rescaleAxes();
    m_plotCorrResults->replot(QCustomPlot::rpQueuedReplot);

    m_plotFiltPLL->graph(0)->setData(t, filtPll, true);
    m_plotFiltPLL->rescaleAxes();
    m_plotFiltPLL->replot(QCustomPlot::rpQueuedReplot);

    m_plotRawDLL->graph(0)->setData(t, rawDll, true);
    m_plotRawDLL->rescaleAxes();
    m_plotRawDLL->replot(QCustomPlot::rpQueuedReplot);

    m_plotFiltDLL->graph(0)->setData(t, filtDll, true);
    m_plotFiltDLL->rescaleAxes();
    m_plotFiltDLL->replot(QCustomPlot::rpQueuedReplot);
}

void SDRPlotWindow::onPrevSatelliteClicked()
{
    if (m_trackResults.empty()) return;
    if (m_currentTrackIndex > 0) {
        updateTrackingChannelView(m_currentTrackIndex - 1);
    }
    else {
        updateTrackingChannelView(m_trackResults.size() - 1);
    }
}

void SDRPlotWindow::onNextSatelliteClicked()
{
    if (m_trackResults.empty()) return;
    if (m_currentTrackIndex + 1 < m_trackResults.size()) {
        updateTrackingChannelView(m_currentTrackIndex + 1);
    }
    else {
        updateTrackingChannelView(0);
    }
}

void SDRPlotWindow::plotNavigationResults(const NavSolutions& navSol, const std::vector<ChannelTrackResult>* trackResults)
{
    // A navigation position fix and a meaningful sky geometry both need at
    // least 4 tracked satellites; with fewer, show a notice instead of plots.
    int trackedCount = 0;
    if (trackResults) {
        for (const auto& tr : *trackResults) {
            if (tr.PRN > 0 && !tr.I_P.empty()) ++trackedCount;
        }
    }
    else {
        trackedCount = static_cast<int>(navSol.activePrns.size());
    }

    const QString noPlotsText = "No Plots since No. of Satellite < 4";
    if (trackedCount < 4) {
        auto addNotice = [](QCustomPlot* plot, const QString& text) {
            QCPItemText* msg = new QCPItemText(plot);
            msg->position->setType(QCPItemPosition::ptViewportRatio);
            msg->position->setCoords(0.5, 0.5);
            msg->setPositionAlignment(Qt::AlignCenter);
            msg->setTextAlignment(Qt::AlignCenter);
            msg->setText(text);
            msg->setFont(QFont("Arial", 11, QFont::Bold));
            msg->setColor(QColor(120, 120, 120));
        };

        m_navTrackPlot->graph(0)->setData(QVector<double>(), QVector<double>());
        m_navTrackPlot->clearItems();
        m_navTrackPlot->xAxis->setRange(-180, 180);
        m_navTrackPlot->yAxis->setRange(-90, 90);
        addNotice(m_navTrackPlot, noPlotsText);
        m_navTrackPlot->replot();

        m_skyPlot->clearPlottables();
        m_skyPlot->clearItems();
        drawSkyPlotBaseGrid();
        addNotice(m_skyPlot, noPlotsText);
        m_skyPlot->xAxis->setRange(-1.15, 1.15);
        m_skyPlot->yAxis->setRange(-1.15, 1.15);
        m_skyPlot->replot();
        return;
    }

    QVector<double> lats, lons;
    for (size_t i = 0; i < navSol.latitude.size(); ++i) {
        if (std::abs(navSol.latitude[i]) > 1e-4) {
            lats.push_back(navSol.latitude[i]);
            lons.push_back(navSol.longitude[i]);
        }
    }
    m_navTrackPlot->clearItems();
    m_navTrackPlot->graph(0)->setData(lons, lats);
    m_navTrackPlot->rescaleAxes();
    m_navTrackPlot->replot();

    m_skyPlot->clearPlottables();
    m_skyPlot->clearItems();
    drawSkyPlotBaseGrid();

    bool hasNavFix = false;
    for (size_t i = 0; i < navSol.latitude.size(); ++i) {
        if (std::abs(navSol.latitude[i]) > 1e-4) { hasNavFix = true; break; }
    }

    // Determine which PRNs to show: prefer nav fix PRNs, fallback to tracked PRNs
    std::vector<int> prnsToShow;
    if (hasNavFix && !navSol.activePrns.empty()) {
        prnsToShow = navSol.activePrns;
    } else if (trackResults && !trackResults->empty()) {
        for (const auto& tr : *trackResults) {
            if (tr.PRN > 0 && !tr.I_P.empty()) prnsToShow.push_back(tr.PRN);
        }
    } else if (!navSol.activePrns.empty()) {
        prnsToShow = navSol.activePrns;
    }

    for (size_t i = 0; i < prnsToShow.size(); ++i) {
        int prn = prnsToShow[i];
        double el = 0.0;
        double azRad = 0.0;
        bool hasElAz = false;
        bool fromNavFix = hasNavFix;
        QColor dotColor = QColor(0, 114, 189); // blue for nav fix
        QColor labelColor = QColor(0, 32, 96);

        // Locate the nav-solution row for this PRN (if present).
        long row = -1;
        for (size_t r = 0; r < navSol.activePrns.size(); ++r) {
            if (navSol.activePrns[r] == prn) { row = static_cast<long>(r); break; }
        }

        // Try to get elevation/azimuth from the nav solution. With a position
        // fix these are true az/el; without one they are the best-effort
        // geocentric az/el computed from the ephemeris (still real geometry).
        if (row >= 0 && row < static_cast<long>(navSol.elevation.size()) &&
            row < static_cast<long>(navSol.azimuth.size())) {
            const auto& elRow = navSol.elevation[row];
            if (!elRow.empty()) {
                int epoch = -1;
                for (int e = static_cast<int>(elRow.size()) - 1; e >= 0; --e) {
                    if (elRow[e] > 0.0) { epoch = e; break; }
                }
                if (epoch >= 0 &&
                    static_cast<int>(navSol.azimuth[row].size()) > epoch) {
                    el = elRow[epoch];
                    azRad = navSol.azimuth[row][epoch] * (M_PI / 180.0);
                    hasElAz = true;
                }
            }
        }

        bool geocentric = hasElAz && !fromNavFix;

        if (!hasElAz) {
            // Fallback: show at center (zenith) with gray color for tracking-only
            el = 90.0;
            azRad = 0.0;
            dotColor = QColor(180, 180, 180);
            labelColor = QColor(100, 100, 100);
        } else if (geocentric) {
            dotColor = QColor(112, 173, 71); // green for geocentric ephem
            labelColor = QColor(40, 90, 30);
        }

        double r = (90.0 - el) / 90.0;
        if (r > 1.0) r = 1.0;

        double x = r * std::sin(azRad);
        double y = r * std::cos(azRad);

        QCPItemEllipse* satDot = new QCPItemEllipse(m_skyPlot);
        satDot->topLeft->setCoords(x - 0.012, y + 0.012);
        satDot->bottomRight->setCoords(x + 0.012, y - 0.012);
        satDot->setBrush(dotColor);
        satDot->setPen(Qt::NoPen);

        QString label = QString("PRN %1").arg(prn);
        if (geocentric) label += " (geo)";
        else if (!hasElAz) label += " (trk)";
        QCPItemText* satLabel = new QCPItemText(m_skyPlot);
        satLabel->position->setCoords(x + 0.045, y + 0.015);
        satLabel->setText(label);
        satLabel->setFont(QFont("Arial", 8, QFont::Bold));
        satLabel->setColor(labelColor);
    }

    m_skyPlot->xAxis->setRange(-1.15, 1.15);
    m_skyPlot->yAxis->setRange(-1.15, 1.15);
    m_skyPlot->replot();
}