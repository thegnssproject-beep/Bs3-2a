#define _USE_MATH_DEFINES
#include <cmath>
#include <fstream>
#include <algorithm>
#include <QMessageBox>
#include "DataProbeDialog.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

DataProbeDialog::DataProbeDialog(const Settings& settings, QWidget* parent)
    : QDialog(parent, Qt::Window | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint)
{
    setWindowTitle("Raw Signal Data Probe & Spectral Analysis");
    resize(950, 750);
    setModal(false);
    setAttribute(Qt::WA_DeleteOnClose, false);
    setupUI();
    loadAndAnalyzeData(settings);
}

void DataProbeDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

#if __has_include("qcustomplot.h")
    QGridLayout* gridLayout = new QGridLayout();

    plotFreq = new QCustomPlot(this);
    plotTime = new QCustomPlot(this);
    plotHist = new QCustomPlot(this);

    gridLayout->addWidget(plotFreq, 0, 0, 1, 2);
    gridLayout->addWidget(plotTime, 1, 0, 1, 1);
    gridLayout->addWidget(plotHist, 1, 1, 1, 1);

    mainLayout->addLayout(gridLayout);
#endif
}

bool DataProbeDialog::loadAndAnalyzeData(const Settings& settings)
{
    std::ifstream fid(settings.fileName.toStdString(), std::ios::binary);
    if (!fid.is_open()) {
        QMessageBox::critical(this, "File Error", "Unable to open raw signal file: " + settings.fileName);
        return false;
    }

    int dataAdaptCoeff = (settings.fileType == 1) ? 1 : 2;
    fid.seekg(dataAdaptCoeff * settings.skipNumberOfBytes, std::ios::beg);

    long long samplesPerCode = static_cast<long long>(std::round(
        settings.samplingFreq / (settings.codeFreqBasis / settings.codeLength)
    ));

    long long totalSamplesToRead = 5 * samplesPerCode;
    long long totalBytesToRead = dataAdaptCoeff * totalSamplesToRead;

    std::vector<int8_t> rawBuffer(totalBytesToRead);
    fid.read(reinterpret_cast<char*>(rawBuffer.data()), totalBytesToRead);
    std::streamsize bytesRead = fid.gcount();
    fid.close();

    if (bytesRead <= 0) return false;
    totalSamplesToRead = bytesRead / dataAdaptCoeff;

    std::vector<double> realData(totalSamplesToRead);
    if (dataAdaptCoeff == 1) {
        for (long long i = 0; i < totalSamplesToRead; ++i) {
            realData[i] = static_cast<double>(rawBuffer[i]);
        }
    }
    else {
        for (long long i = 0; i < totalSamplesToRead; ++i) {
            realData[i] = static_cast<double>(rawBuffer[2 * i]);
        }
    }

#if __has_include("qcustomplot.h")
    // 1. Time Domain Plot
    size_t displaySamples = std::min<size_t>(static_cast<size_t>(totalSamplesToRead), static_cast<size_t>(std::max(10LL, samplesPerCode / 500)));
    QVector<double> timeMs(displaySamples), ampData(displaySamples);

    for (size_t i = 0; i < displaySamples; ++i) {
        timeMs[i] = (static_cast<double>(i) / settings.samplingFreq) * 1000.0;
        ampData[i] = realData[i];
    }

    plotTime->clearGraphs();
    plotTime->addGraph();
    plotTime->graph(0)->setData(timeMs, ampData);
    plotTime->graph(0)->setPen(QPen(QColor(0, 114, 189), 1.2));
    plotTime->xAxis->setLabel("Time (ms)");
    plotTime->yAxis->setLabel("Amplitude");
    plotTime->yAxis->setRange(-3.5, 3.5);
    plotTime->xAxis->setRange(0, timeMs.last());
    plotTime->replot();

    // 2. Frequency Domain Spectrum (Matching MATLAB probeData resolution & Nyquist band)
    const int nFft = 8192;
    int numBins = nFft / 2;
    int numSegments = static_cast<int>(totalSamplesToRead / nFft);
    QVector<double> avgPsd(numBins, 0.0);

    for (int seg = 0; seg < numSegments; ++seg) {
        for (int k = 0; k < numBins; ++k) {
            double sumR = 0.0, sumI = 0.0;
            for (int n = 0; n < nFft; ++n) {
                double angle = -2.0 * M_PI * k * n / nFft;
                double val = realData[seg * nFft + n];
                sumR += val * std::cos(angle);
                sumI += val * std::sin(angle);
            }
            avgPsd[k] += (sumR * sumR + sumI * sumI);
        }
    }

    double maxVal = 1e-12;
    for (double p : avgPsd) if (p > maxVal) maxVal = p;

    QVector<double> freqMHz(numBins), magDb(numBins);
    for (int k = 0; k < numBins; ++k) {
        freqMHz[k] = (k * (settings.samplingFreq / nFft)) / 1.0e6;
        magDb[k] = 10.0 * std::log10((avgPsd[k] / maxVal) + 1e-6);
    }

    plotFreq->clearGraphs();
    plotFreq->addGraph();
    plotFreq->graph(0)->setData(freqMHz, magDb);
    plotFreq->graph(0)->setPen(QPen(QColor(0, 114, 189), 1.0));
    plotFreq->xAxis->setLabel("Frequency (MHz)");
    plotFreq->yAxis->setLabel("Magnitude");
    plotFreq->xAxis->setRange(0, (settings.samplingFreq / 2.0) / 1.0e6);
    plotFreq->yAxis->setRange(-26.0, 4.0);
    plotFreq->replot();

    // 3. Histogram
    QVector<double> bins = { -3.0, -1.0, 1.0, 3.0 };
    QVector<double> counts(4, 0.0);

    for (size_t i = 0; i < static_cast<size_t>(totalSamplesToRead); ++i) {
        double val = realData[i];
        if (val <= -2.0) counts[0]++;
        else if (val < 0.0) counts[1]++;
        else if (val < 2.0) counts[2]++;
        else counts[3]++;
    }

    plotHist->clearPlottables();
    QCPBars* histBars = new QCPBars(plotHist->xAxis, plotHist->yAxis);
    histBars->setData(bins, counts);
    histBars->setWidth(0.85);
    histBars->setPen(QPen(Qt::black));
    histBars->setBrush(QColor(50, 40, 160));

    plotHist->xAxis->setLabel("Bin");
    plotHist->yAxis->setLabel("Number in bin");
    plotHist->xAxis->setRange(-4.0, 4.0);
    plotHist->yAxis->setRange(0, 190000);
    plotHist->replot();
#endif

    return true;
}

bool DataProbeDialog::inspect(const Settings& settings, QWidget* /*parent*/)
{
    // Passing nullptr gives it its own independent top-level window and taskbar entry
    DataProbeDialog* dialog = new DataProbeDialog(settings, nullptr);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
    return true;
}