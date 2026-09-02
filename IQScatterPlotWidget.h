#pragma once

#include <QWidget>
#include <vector>
#include <QString>
#include <QPoint>
#include "Tracking.h"

class IQScatterPlotWidget : public QWidget
{
    Q_OBJECT

public:
    explicit IQScatterPlotWidget(QWidget* parent = nullptr);

    // Updates the scatter constellation plot with tracking data
    void setTrackingData(const std::vector<double>& iPrompt,
        const std::vector<double>& qPrompt,
        int prn = 0);

    // Convenience method using ChannelTrackResult
    void setChannelData(const ChannelTrackResult& trackRes);

    // Clears the plot
    void clear();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    QPointF screenToData(const QPoint& pos, const QRect& plotRect,
        int centerX, int centerY, int halfW, int halfH) const;

    std::vector<double> m_iPrompt;
    std::vector<double> m_qPrompt;
    int m_prn = 0;
    double m_maxRange = 6000.0; // Default axis limit matching MATLAB

    double m_zoom = 1.0;        // 1.0 = auto-range (data normalized to plot)
    double m_centerX = 0.0;     // data coordinates of plot center along I
    double m_centerY = 0.0;     // data coordinates of plot center along Q
    bool m_dragging = false;
    QPoint m_lastDragPos;
};