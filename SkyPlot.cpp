#include "SkyPlot.h"
#include <cmath>
#include <QFont>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void SkyPlot::draw(QCustomPlot* customPlot,
    const std::vector<std::vector<double>>& azimuth,
    const std::vector<std::vector<double>>& elevation,
    const std::vector<int>& prnList)
{
#if __has_include("qcustomplot.h")
    if (!customPlot) return;

    customPlot->clearGraphs();
    customPlot->clearPlottables();
    customPlot->clearItems();

    customPlot->xAxis->setVisible(false);
    customPlot->yAxis->setVisible(false);
    customPlot->xAxis->setRange(-1.25, 1.25);
    customPlot->yAxis->setRange(-1.25, 1.25);

    // 1. Draw Concentric Elevation Rings (0°, 30°, 60°)
    const double elRings[] = { 0.0, 30.0, 60.0 };
    for (double el : elRings) {
        double r = (90.0 - el) / 90.0;
        QCPCurve* ring = new QCPCurve(customPlot->xAxis, customPlot->yAxis);
        QVector<QCPCurveData> ringData(101);
        for (int k = 0; k <= 100; ++k) {
            double theta = 2.0 * M_PI * k / 100.0;
            ringData[k] = QCPCurveData(k, r * std::cos(theta), r * std::sin(theta));
        }
        ring->data()->set(ringData, true);
        ring->setPen(QPen(Qt::lightGray, 1, Qt::DashLine));

        // Elevation degree labels
        QCPItemText* elLabel = new QCPItemText(customPlot);
        elLabel->position->setCoords(0.04, r);
        elLabel->setText(QString("%1°").arg(static_cast<int>(el)));
        elLabel->setColor(Qt::darkGray);
        elLabel->setFont(QFont("sans", 7));
    }

    // 2. Draw 30° Azimuth Spokes & Cardinal Labels
    for (int deg = 0; deg < 360; deg += 30) {
        double rad = (90.0 - deg) * M_PI / 180.0;
        QCPCurve* spoke = new QCPCurve(customPlot->xAxis, customPlot->yAxis);
        QVector<QCPCurveData> spokeData = {
            QCPCurveData(0, 0.0, 0.0),
            QCPCurveData(1, std::cos(rad), std::sin(rad))
        };
        spoke->data()->set(spokeData, true);
        spoke->setPen(QPen(QColor(220, 220, 220), 1, Qt::DotLine));

        // Outer degree labels
        QCPItemText* azLabel = new QCPItemText(customPlot);
        azLabel->position->setCoords(1.12 * std::cos(rad), 1.12 * std::sin(rad));
        azLabel->setText(QString("%1°").arg(deg));
        azLabel->setColor(QColor(70, 70, 70));
        azLabel->setFont(QFont("sans", 8, QFont::Bold));
    }

    // 3. Draw Tracked Satellites & Trajectories
    for (size_t i = 0; i < prnList.size(); ++i) {
        if (i >= azimuth.size() || i >= elevation.size()) continue;
        if (azimuth[i].empty() || elevation[i].empty()) continue;

        int prn = prnList[i];
        double azDeg = azimuth[i].back();
        double elDeg = elevation[i].back();

        if (elDeg < 0.0) continue;

        // Polar to Cartesian conversion (North up = 0°)
        double r = (90.0 - elDeg) / 90.0;
        double theta = (90.0 - azDeg) * M_PI / 180.0;
        double x = r * std::cos(theta);
        double y = r * std::sin(theta);

        // Satellite Marker (Green filled circle)
        QCPItemEllipse* satCircle = new QCPItemEllipse(customPlot);
        satCircle->topLeft->setCoords(x - 0.055, y + 0.055);
        satCircle->bottomRight->setCoords(x + 0.055, y - 0.055);
        satCircle->setBrush(QColor(40, 180, 40));
        satCircle->setPen(QPen(Qt::black, 1.5));

        // PRN Text Inside Marker
        QCPItemText* prnText = new QCPItemText(customPlot);
        prnText->position->setCoords(x, y);
        prnText->setText(QString::number(prn));
        prnText->setColor(Qt::white);
        prnText->setFont(QFont("sans", 8, QFont::Bold));
    }

    customPlot->replot();
#endif
}