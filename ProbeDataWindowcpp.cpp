#include "ProbeDataWindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QWidget>
#include <QFont>
#include <QPen>
#include <QBrush>
#include <cmath>
#include <algorithm>
#include <map>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

ProbeDataWindow::ProbeDataWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setupUI();
    setWindowTitle("BDS-3 B2a Raw IF Probe Data & Spectrum Analysis (Figure 100)");
    resize(1100, 750);
}

void ProbeDataWindow::setupUI()
{
    QWidget* central = new QWidget(this);
    setCentralWidget(central);
    QGridLayout* layout = new QGridLayout(central);

#if __has_include("qcustomplot.h")
    plotPSD = new QCustomPlot(this);
    plotTime = new QCustomPlot(this);
    plotHist = new QCustomPlot(this);

    plotPSD->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    plotTime->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    plotHist->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);

    // Top: Frequency Domain Plot across span
    layout->addWidget(plotPSD, 0, 0, 1, 2);
    // Bottom Left: Time domain plot
    layout->addWidget(plotTime, 1, 0, 1, 1);
    // Bottom Right: Histogram
    layout->addWidget(plotHist, 1, 1, 1, 1);

    layout->setRowStretch(0, 3);
    layout->setRowStretch(1, 2);
#endif
}

void ProbeDataWindow::plotProbeData(const std::vector<std::complex<double>>& rawSignal, double fs, int fileType)
{
#if __has_include("qcustomplot.h")
    if (!plotPSD || !plotTime || !plotHist || rawSignal.empty()) return;

    // =========================================================================
    // 1. FREQUENCY DOMAIN (PSD - Welch Average matching MATLAB)
    // =========================================================================
    plotPSD->clearGraphs();
    int nFFT = 4096;
    int samplesToUse = static_cast<int>(rawSignal.size());
    int numWindows = std::max(1, samplesToUse / nFFT);

    QVector<double> freqMHz(nFFT / 2), psdDb(nFFT / 2, 0.0);
    for (int k = 0; k < nFFT / 2; ++k) {
        freqMHz[k] = (k * (fs / 2.0) / (nFFT / 2)) / 1e6;
    }

    for (int w = 0; w < numWindows; ++w) {
        for (int k = 0; k < nFFT / 2; ++k) {
            std::complex<double> sum(0, 0);
            for (int n = 0; n < nFFT; ++n) {
                double angle = -2.0 * M_PI * k * n / nFFT;
                double hanning = 0.5 * (1.0 - std::cos(2.0 * M_PI * n / (nFFT - 1)));
                std::complex<double> sample = rawSignal[w * nFFT + n] * hanning;
                sum += sample * std::complex<double>(std::cos(angle), std::sin(angle));
            }
            psdDb[k] += std::norm(sum);
        }
    }

    double maxPsd = -1e9;
    for (int k = 0; k < nFFT / 2; ++k) {
        psdDb[k] = 10.0 * std::log10((psdDb[k] / numWindows) + 1e-12);
        if (psdDb[k] > maxPsd) maxPsd = psdDb[k];
    }
    for (int k = 0; k < nFFT / 2; ++k) {
        psdDb[k] -= maxPsd; // Normalize top peak to 0 dB
    }

    plotPSD->addGraph();
    plotPSD->graph(0)->setData(freqMHz, psdDb);
    plotPSD->graph(0)->setPen(QPen(QColor(0, 114, 189), 1.0));
    plotPSD->xAxis->setLabel("Frequency (MHz)");
    plotPSD->yAxis->setLabel("Magnitude (dB)");
    plotPSD->xAxis->setRange(0, (fs / 2.0) / 1e6);
    plotPSD->yAxis->setRange(-25.0, 5.0);
    plotPSD->plotLayout()->insertRow(0);
    plotPSD->plotLayout()->addElement(0, 0, new QCPTextElement(plotPSD, "Frequency domain plot", QFont("Arial", 10, QFont::Bold)));
    plotPSD->replot();

    // =========================================================================
    // 2. TIME DOMAIN PLOT (First ~200 samples / 2 us)
    // =========================================================================
    plotTime->clearGraphs();
    int timeSamples = std::min<int>(200, static_cast<int>(rawSignal.size()));
    QVector<double> timeMs(timeSamples), timeAmp(timeSamples);

    for (int i = 0; i < timeSamples; ++i) {
        timeMs[i] = (i / fs) * 1000.0;
        timeAmp[i] = rawSignal[i].real();
    }

    plotTime->addGraph();
    plotTime->graph(0)->setData(timeMs, timeAmp);
    plotTime->graph(0)->setPen(QPen(QColor(0, 114, 189), 1.2));
    plotTime->xAxis->setLabel("Time (ms)");
    plotTime->yAxis->setLabel("Amplitude");
    plotTime->xAxis->setRange(0, timeMs.back());
    plotTime->yAxis->setRange(-3.5, 3.5);
    plotTime->plotLayout()->insertRow(0);
    plotTime->plotLayout()->addElement(0, 0, new QCPTextElement(plotTime, "Time domain plot", QFont("Arial", 10, QFont::Bold)));
    plotTime->replot();

    // =========================================================================
    // 3. ADC HISTOGRAM
    // =========================================================================
    plotHist->clearPlottables();
    std::map<int, int> binCounts;
    for (size_t i = 0; i < rawSignal.size(); ++i) {
        int val = static_cast<int>(std::round(rawSignal[i].real()));
        binCounts[val]++;
    }

    QCPBars* histBars = new QCPBars(plotHist->xAxis, plotHist->yAxis);
    histBars->setBrush(QBrush(QColor(50, 40, 160)));
    histBars->setPen(QPen(QColor(30, 20, 120)));
    histBars->setWidth(0.8);

    QVector<double> ticks;
    QVector<double> counts;
    for (int b = -4; b <= 4; ++b) {
        ticks.push_back(b);
        counts.push_back(binCounts[b]);
    }

    histBars->setData(ticks, counts);
    plotHist->xAxis->setLabel("Bin");
    plotHist->yAxis->setLabel("Number in bin");
    plotHist->xAxis->setRange(-4.5, 4.5);
    plotHist->yAxis->rescale(true);
    plotHist->plotLayout()->insertRow(0);
    plotHist->plotLayout()->addElement(0, 0, new QCPTextElement(plotHist, "Histogram", QFont("Arial", 10, QFont::Bold)));
    plotHist->replot();
#endif
}