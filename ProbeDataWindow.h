#pragma once
#include <QMainWindow>
#include <vector>
#include <complex>

#if __has_include("qcustomplot.h")
#include "qcustomplot.h"
#endif

class ProbeDataWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit ProbeDataWindow(QWidget* parent = nullptr);
    void plotProbeData(const std::vector<std::complex<double>>& rawSignal, double fs, int fileType = 1);

private:
    void setupUI();

#if __has_include("qcustomplot.h")
    QCustomPlot* plotPSD = nullptr;
    QCustomPlot* plotTime = nullptr;
    QCustomPlot* plotHist = nullptr;
#endif
};