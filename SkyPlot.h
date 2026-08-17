#pragma once

#include <vector>
#include <cmath>
#include <QString>
#include <QColor>

#if __has_include("qcustomplot.h")
#include "qcustomplot.h"
#endif

class SkyPlot
{
public:
    // Renders the sky view plot onto a QCustomPlot widget
    // Inputs:
    //   plotWidget - Target QCustomPlot instance
    //   az         - 2D vector [satIndex][timeIndex] of satellite Azimuth angles (degrees)
    //   el         - 2D vector [satIndex][timeIndex] of satellite Elevation angles (degrees)
    //   prnList    - Vector of active satellite PRNs
    static void draw(
#if __has_include("qcustomplot.h")
        QCustomPlot* plotWidget,
#else
        void* plotWidget,
#endif
        const std::vector<std::vector<double>>& az,
        const std::vector<std::vector<double>>& el,
        const std::vector<int>& prnList
    );

private:
#if __has_include("qcustomplot.h")
    static void drawPolarGrid(QCustomPlot* customPlot);
#endif
};