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
    : QDialog(parent)
{
    setWindowTitle("Raw Signal Data Probe & Spectral Analysis");
    resize(950, 750);
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

    // Layout matching MATLAB probeData figure
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

    std::vector<std::complex<double>> signalData(totalSamplesToRead);
    if (dataAdaptCoeff == 1) {
        for (long long i = 0; i < totalSamplesToRead; ++i) {
            signalData[i] = std::complex<double>(static_cast<double>(rawBuffer[i]), 0.0);
        }
    }
    else {
        for (long long i = 0; i < totalSamplesToRead; ++i) {
            double I = static_cast<double>(rawBuffer[2 * i]);
            double Q = static_cast<double>(rawBuffer[2 * i + 1]);
            signalData[i] = std::complex<double>(I, Q);
        }
    }

#if __has_include("qcustomplot.h")
    // 1. Time Domain Plot (~samplesPerCode / 500 samples like MATLAB)
    size_t displaySamples = std::min<size_t>(static_cast<size_t>(totalSamplesToRead), static_cast<size_t>(std::max(10LL, samplesPerCode / 500)));
    QVector<double> timeMs(displaySamples), I_data(displaySamples);

    for (size_t i = 0; i < displaySamples; ++i) {
        timeMs[i] = (static_cast<double>(i) / settings.samplingFreq) * 1000.0;
        I_data[i] = signalData[i].real();
    }

    plotTime->clearGraphs();
    plotTime->addGraph();
    plotTime->graph(0)->setData(timeMs, I_data);
    plotTime->graph(0)->setPen(QPen(Qt::blue, 1.5));
    plotTime->xAxis->setLabel("Time (ms)");
    plotTime->yAxis->setLabel("Amplitude");
    plotTime->yAxis->setRange(-3.5, 3.5);
    plotTime->xAxis->setRange(0, timeMs.last());

    if (plotTime->plotLayout()->rowCount() == 1) {
        plotTime->plotLayout()->insertRow(0);
        plotTime->plotLayout()->addElement(0, 0, new QCPTextElement(plotTime, "Time domain plot", QFont("sans", 10, QFont::Bold)));
    }
    plotTime->replot();

    // 2. Frequency Domain Spectrum (Welch PSD Estimate)
    size_t nFft = 32768;
    if (signalData.size() >= nFft) {
        QVector<double> freqMHz(nFft / 4), psdDb(nFft / 4);
        for (size_t k = 0; k < nFft / 4; ++k) {
            freqMHz[k] = (static_cast<double>(k) * (settings.samplingFreq / nFft)) / 1.0e6;

            std::complex<double> sumVal(0.0, 0.0);
            for (size_t n = 0; n < 512; ++n) {
                double angle = -2.0 * M_PI * k * n / nFft;
                sumVal += signalData[n] * std::complex<double>(std::cos(angle), std::sin(angle));
            }
            double power = (std::norm(sumVal) / 512.0) + 1e-12;
            psdDb[k] = 10.0 * std::log10(power);
        }

        plotFreq->clearGraphs();
        plotFreq->addGraph();
        plotFreq->graph(0)->setData(freqMHz, psdDb);
        plotFreq->graph(0)->setPen(QPen(QColor(0, 114, 189), 1.0));
        plotFreq->xAxis->setLabel("Frequency (MHz)");
        plotFreq->yAxis->setLabel("Magnitude");
        plotFreq->xAxis->setRange(0, (settings.samplingFreq / 2.0) / 1.0e6);
        plotFreq->rescaleAxes();

        if (plotFreq->plotLayout()->rowCount() == 1) {
            plotFreq->plotLayout()->insertRow(0);
            plotFreq->plotLayout()->addElement(0, 0, new QCPTextElement(plotFreq, "Frequency domain plot", QFont("sans", 10, QFont::Bold)));
        }
        plotFreq->replot();
    }

    // 3. ADC Level Histogram (Discrete bins: -3, -1, 1, 3)
    QVector<double> bins = { -3.0, -1.0, 1.0, 3.0 };
    QVector<double> counts(4, 0.0);

    for (size_t i = 0; i < static_cast<size_t>(totalSamplesToRead); ++i) {
        double val = signalData[i].real();
        if (val <= -2.0) counts[0]++;
        else if (val < 0.0) counts[1]++;
        else if (val < 2.0) counts[2]++;
        else counts[3]++;
    }

    plotHist->clearPlottables();
    QCPBars* histBars = new QCPBars(plotHist->xAxis, plotHist->yAxis);
    histBars->setData(bins, counts);
    histBars->setWidth(0.8);
    histBars->setPen(QPen(Qt::black));
    histBars->setBrush(QColor(50, 40, 160));

    plotHist->xAxis->setLabel("Bin");
    plotHist->yAxis->setLabel("Number in bin");
    plotHist->xAxis->setRange(-4.5, 4.5);
    plotHist->rescaleAxes();

    if (plotHist->plotLayout()->rowCount() == 1) {
        plotHist->plotLayout()->insertRow(0);
        plotHist->plotLayout()->addElement(0, 0, new QCPTextElement(plotHist, "Histogram", QFont("sans", 10, QFont::Bold)));
    }
    plotHist->replot();
#endif

    return true;
}

bool DataProbeDialog::inspect(const Settings& settings, QWidget* parent)
{
    DataProbeDialog dialog(settings, parent);
    return dialog.exec() == QDialog::Accepted;
}