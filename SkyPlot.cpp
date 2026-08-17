#define _USE_MATH_DEFINES
#include <cmath>
#include "SkyPlot.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#if __has_include("qcustomplot.h")

void SkyPlot::drawPolarGrid(QCustomPlot* customPlot)
{
    customPlot->clearGraphs();
    customPlot->clearItems();
    customPlot->clearPlottables();

    // 1. Concentric Elevation Circles (0, 15, 30, 45, 60, 75, 90 degrees)
    const int numCirclePoints = 120;
    for (int elevation = 0; elevation <= 90; elevation += 15) {
        double r = 90.0 * std::cos(elevation * M_PI / 180.0);

        QVector<double> xCircle(numCirclePoints + 1), yCircle(numCirclePoints + 1);
        for (int i = 0; i <= numCirclePoints; ++i) {
            double angle = i * 2.0 * M_PI / numCirclePoints;
            xCircle[i] = r * std::sin(angle);
            yCircle[i] = r * std::cos(angle);
        }

        QCPCurve* ring = new QCPCurve(customPlot->xAxis, customPlot->yAxis);
        ring->setData(xCircle, yCircle);
        ring->setPen(QPen(QColor(180, 180, 180), 1, Qt::DotLine));

        // Elevation Text Labels
        if (elevation < 90) {
            QCPItemText* elText = new QCPItemText(customPlot);
            elText->position->setCoords(0, r);
            elText->setText(QString::number(elevation) + "°");
            elText->setFont(QFont("sans", 8));
            elText->setColor(QColor(100, 100, 100));
        }
    }

    // 2. Azimuth Radial Spokes (every 30 degrees)
    for (int deg = 0; deg < 360; deg += 30) {
        double rad = deg * M_PI / 180.0;
        double endX = 90.0 * std::sin(rad);
        double endY = 90.0 * std::cos(rad);

        QCPCurve* spoke = new QCPCurve(customPlot->xAxis, customPlot->yAxis);
        QVector<double> spokeX = { 0.0, endX };
        QVector<double> spokeY = { 0.0, endY };
        spoke->setData(spokeX, spokeY);
        spoke->setPen(QPen(QColor(200, 200, 200), 1, Qt::DotLine));

        // Outer Azimuth Degree Labels
        double labelR = 100.0;
        QCPItemText* azText = new QCPItemText(customPlot);
        azText->position->setCoords(labelR * std::sin(rad), labelR * std::cos(rad));
        azText->setText(QString::number(deg) + "°");
        azText->setFont(QFont("sans", 8, QFont::Bold));
        azText->setColor(QColor(60, 60, 60));
    }
}

void SkyPlot::draw(
    QCustomPlot* plotWidget,
    const std::vector<std::vector<double>>& az,
    const std::vector<std::vector<double>>& el,
    const std::vector<int>& prnList)
{
    if (!plotWidget) return;

    drawPolarGrid(plotWidget);

    size_t numSats = prnList.size();
    if (az.size() < numSats || el.size() < numSats) return;

    // Distinct palette for satellite trajectory lines
    const QVector<QColor> colors = {
        Qt::blue, Qt::red, Qt::darkGreen, Qt::magenta,
        Qt::darkCyan, Qt::darkYellow, Qt::darkRed, Qt::darkBlue
    };

    for (size_t i = 0; i < numSats; ++i) {
        int prn = prnList[i];
        if (prn <= 0 || az[i].empty()) continue;

        size_t numPoints = az[i].size();
        QVector<double> xTrack, yTrack;

        for (size_t k = 0; k < numPoints; ++k) {
            double curAz = az[i][k];
            double curEl = el[i][k];

            if (curEl < 0.0) continue; // Below horizon

            double r = 90.0 * std::cos(curEl * M_PI / 180.0);
            xTrack.append(r * std::sin(curAz * M_PI / 180.0));
            yTrack.append(r * std::cos(curAz * M_PI / 180.0));
        }

        if (xTrack.isEmpty()) continue;

        QColor satColor = colors[i % colors.size()];

        // Draw Trajectory Track
        QCPCurve* trackCurve = new QCPCurve(plotWidget->xAxis, plotWidget->yAxis);
        trackCurve->setData(xTrack, yTrack);
        trackCurve->setPen(QPen(satColor, 2));

        // Mark Latest Position
        QCPCurve* currentPos = new QCPCurve(plotWidget->xAxis, plotWidget->yAxis);
        QVector<double> latestX = { xTrack.last() };
        QVector<double> latestY = { yTrack.last() };
        currentPos->setData(latestX, latestY);
        currentPos->setPen(QPen(satColor, 2));
        currentPos->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, satColor, Qt::white, 8));

        // Place Satellite PRN Text Annotation
        QCPItemText* prnLabel = new QCPItemText(plotWidget);
        prnLabel->position->setCoords(xTrack.last() + 4.0, yTrack.last() + 4.0);
        prnLabel->setText(QString("C%1").arg(prn, 2, 10, QChar('0')));
        prnLabel->setFont(QFont("sans", 9, QFont::Bold));
        prnLabel->setColor(satColor);
    }

    // Configure Polar Bounds and Aspect Ratio
    plotWidget->xAxis->setRange(-115, 115);
    plotWidget->yAxis->setRange(-115, 115);
    plotWidget->xAxis->setVisible(false);
    plotWidget->yAxis->setVisible(false);
    plotWidget->replot();
}

#else

void SkyPlot::draw(void*, const std::vector<std::vector<double>>&, const std::vector<std::vector<double>>&, const std::vector<int>&)
{
    // Fallback stub when QCustomPlot is not included
}

#endif