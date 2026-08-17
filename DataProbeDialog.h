#pragma once

#include <QDialog>
#include <QVBoxLayout>
#include <QGridLayout>
#include <vector>
#include <complex>
#include "Settings.h"

#if __has_include("qcustomplot.h")
#include "qcustomplot.h"
#endif

class DataProbeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DataProbeDialog(const Settings& settings, QWidget* parent = nullptr);

    static bool inspect(const Settings& settings, QWidget* parent = nullptr);

private:
    void setupUI();
    bool loadAndAnalyzeData(const Settings& settings);

#if __has_include("qcustomplot.h")
    QCustomPlot* plotTime;
    QCustomPlot* plotFreq;
    QCustomPlot* plotHist;
#endif
};