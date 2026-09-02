#include "IQScatterPlotWidget.h"
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

IQScatterPlotWidget::IQScatterPlotWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(280, 260);
    setStyleSheet("background-color: #FFFFFF;");
}

void IQScatterPlotWidget::setTrackingData(const std::vector<double>& iPrompt,
    const std::vector<double>& qPrompt,
    int prn)
{
    m_iPrompt = iPrompt;
    m_qPrompt = qPrompt;
    m_prn = prn;

    // Compute dynamic symmetric range (discard initial loop transient)
    double maxVal = 1000.0;
    size_t startSample = (m_iPrompt.size() > 500) ? 500 : 0;

    for (size_t i = startSample; i < m_iPrompt.size(); ++i) {
        maxVal = std::max(maxVal, std::abs(m_iPrompt[i]));
        if (i < m_qPrompt.size()) {
            maxVal = std::max(maxVal, std::abs(m_qPrompt[i]));
        }
    }

    // Round up to clean scale steps (e.g., 2000, 4000, 6000)
    m_maxRange = std::ceil((maxVal * 1.15) / 1000.0) * 1000.0;
    if (m_maxRange < 2000.0) m_maxRange = 2000.0;

    update();
}

void IQScatterPlotWidget::setChannelData(const ChannelTrackResult& trackRes)
{
    setTrackingData(trackRes.I_P, trackRes.Q_P, trackRes.PRN);
}

void IQScatterPlotWidget::clear()
{
    m_iPrompt.clear();
    m_qPrompt.clear();
    m_prn = 0;
    update();
}

void IQScatterPlotWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QRect totalRect = rect();
    painter.fillRect(totalRect, Qt::white);

    // Layout Margins
    int leftMargin = 55;
    int rightMargin = 25;
    int topMargin = 35;
    int bottomMargin = 45;

    QRect plotRect(leftMargin, topMargin,
        totalRect.width() - leftMargin - rightMargin,
        totalRect.height() - topMargin - bottomMargin);

    if (plotRect.width() <= 10 || plotRect.height() <= 10) return;

    // 1. Draw Title
    painter.setPen(Qt::black);
    QFont titleFont = font();
    titleFont.setBold(true);
    titleFont.setPointSize(10);
    painter.setFont(titleFont);

    QString title = (m_prn > 0)
        ? QString("Discrete-Time Scatter Plot (PRN %1)").arg(m_prn)
        : QString("Discrete-Time Scatter Plot");
    painter.drawText(QRect(0, 5, totalRect.width(), 25), Qt::AlignCenter, title);

    // 2. Draw Plot Box & Grid
    painter.setPen(QPen(QColor(210, 210, 210), 1, Qt::DashLine));

    // Center Axes
    int centerX = plotRect.left() + plotRect.width() / 2;
    int centerY = plotRect.top() + plotRect.height() / 2;
    painter.drawLine(centerX, plotRect.top(), centerX, plotRect.bottom());
    painter.drawLine(plotRect.left(), centerY, plotRect.right(), centerY);

    // Outer Border
    painter.setPen(QPen(Qt::black, 1.2, Qt::SolidLine));
    painter.drawRect(plotRect);

    // 3. Axis Tick Marks & Labels
    QFont tickFont = font();
    tickFont.setPointSize(8);
    painter.setFont(tickFont);

    // X-Axis Labels (-Range, 0, +Range)
    painter.drawText(plotRect.left() - 25, plotRect.bottom() + 5, 50, 15, Qt::AlignCenter, QString::number(-m_maxRange, 'f', 0));
    painter.drawText(centerX - 20, plotRect.bottom() + 5, 40, 15, Qt::AlignCenter, "0");
    painter.drawText(plotRect.right() - 25, plotRect.bottom() + 5, 50, 15, Qt::AlignCenter, QString::number(m_maxRange, 'f', 0));

    // Y-Axis Labels (-Range, 0, +Range)
    painter.drawText(5, plotRect.top() - 7, leftMargin - 10, 15, Qt::AlignRight | Qt::AlignVCenter, QString::number(m_maxRange, 'f', 0));
    painter.drawText(5, centerY - 7, leftMargin - 10, 15, Qt::AlignRight | Qt::AlignVCenter, "0");
    painter.drawText(5, plotRect.bottom() - 7, leftMargin - 10, 15, Qt::AlignRight | Qt::AlignVCenter, QString::number(-m_maxRange, 'f', 0));

    // Axis Titles
    QFont labelFont = font();
    labelFont.setPointSize(9);
    painter.setFont(labelFont);
    painter.drawText(QRect(plotRect.left(), plotRect.bottom() + 20, plotRect.width(), 20), Qt::AlignCenter, "I prompt");

    painter.save();
    painter.translate(15, centerY);
    painter.rotate(-90);
    painter.drawText(QRect(-60, -10, 120, 20), Qt::AlignCenter, "Q prompt");
    painter.restore();

    // 4. Draw I/Q Constellation Scatter Points (Skip transient 0-500 ms)
    if (m_iPrompt.empty() || m_qPrompt.empty()) return;

    double halfW = plotRect.width() / 2.0;
    double halfH = plotRect.height() / 2.0;
    // Data half-range currently visible (shrinks as you zoom in)
    double dataHalfW = m_maxRange / m_zoom;
    double dataHalfH = m_maxRange / m_zoom;

    painter.setClipRect(plotRect);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 114, 189, 180)); // MATLAB blue scatter color

    size_t startIdx = (m_iPrompt.size() > 500) ? 500 : 0;
    size_t count = std::min(m_iPrompt.size(), m_qPrompt.size());

    for (size_t i = startIdx; i < count; ++i) {
        double xData = m_iPrompt[i];
        double yData = m_qPrompt[i];

        // Visible-window check so out-of-view points don't get painted on the pane
        if (xData < m_centerX - dataHalfW || xData > m_centerX + dataHalfW) continue;
        if (yData < m_centerY - dataHalfH || yData > m_centerY + dataHalfH) continue;

        int px = centerX + static_cast<int>((xData - m_centerX) / dataHalfW * halfW);
        int py = centerY - static_cast<int>((yData - m_centerY) / dataHalfH * halfH);

        painter.drawEllipse(QPoint(px, py), 2, 2);
    }
}

void IQScatterPlotWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_lastDragPos = event->pos();
        setCursor(Qt::ClosedHandCursor);
    }
    QWidget::mousePressEvent(event);
}

void IQScatterPlotWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging) {
        // Convert the pixel delta into data-space delta at the current zoom level
        QRect totalRect = rect();
        int leftMargin = 55;
        int rightMargin = 25;
        int topMargin = 35;
        int bottomMargin = 45;

        const int plotW = totalRect.width() - leftMargin - rightMargin;
        const int plotH = totalRect.height() - topMargin - bottomMargin;
        if (plotW <= 10 || plotH <= 10) { m_lastDragPos = event->pos(); return; }

        double dataPerPixelX = (2.0 * (m_maxRange / m_zoom)) / plotW;
        double dataPerPixelY = (2.0 * (m_maxRange / m_zoom)) / plotH;

        QPoint delta = event->pos() - m_lastDragPos;
        // Dragging right should move the view right: pan center opposite to cursor
        m_centerX -= delta.x() * dataPerPixelX;
        m_centerY += delta.y() * dataPerPixelY;

        m_lastDragPos = event->pos();
        update();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void IQScatterPlotWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_dragging) {
        m_dragging = false;
        unsetCursor();
    }
    QWidget::mouseReleaseEvent(event);
}

void IQScatterPlotWidget::wheelEvent(QWheelEvent* event)
{
    double delta = event->angleDelta().y();
    if (delta == 0) { QWidget::wheelEvent(event); return; }

    double zoomFactor = std::pow(1.0015, delta); // wheel-up -> zoom in
    m_zoom = std::max(1.0, m_zoom * zoomFactor);

    // Keep the cursor position fixed under the mouse (zoom toward the cursor)
    QRect totalRect = rect();
    int leftMargin = 55;
    int rightMargin = 25;
    int topMargin = 35;
    int bottomMargin = 45;
    const QRect plotRect(leftMargin, topMargin,
        totalRect.width() - leftMargin - rightMargin,
        totalRect.height() - topMargin - bottomMargin);
    if (plotRect.width() <= 10 || plotRect.height() <= 10) return;

    int centerX = plotRect.left() + plotRect.width() / 2;
    int centerY = plotRect.top() + plotRect.height() / 2;
    double halfW = plotRect.width() / 2.0;
    double halfH = plotRect.height() / 2.0;

    QPointF before = screenToData(event->position().toPoint(), plotRect, centerX, centerY,
        static_cast<int>(halfW), static_cast<int>(halfH));

    double oldDataHalfW = m_maxRange / (m_zoom / zoomFactor); // range before this wheel step
    double oldDataHalfH = oldDataHalfW;
    double newDataHalfW = m_maxRange / m_zoom;
    double newDataHalfH = newDataHalfW;

    // Data-space position under cursor at old range
    double oldX = before.x();
    double oldY = before.y();

    // Recompute so that data point 'before' stays under the cursor after zoom
    m_centerX = oldX - (before.x() - m_centerX) * (newDataHalfW / oldDataHalfW);
    m_centerY = oldY - (before.y() - m_centerY) * (newDataHalfH / oldDataHalfH);

    update();
    QWidget::wheelEvent(event);
}

void IQScatterPlotWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_zoom = 1.0;
        m_centerX = 0.0;
        m_centerY = 0.0;
        update();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

QPointF IQScatterPlotWidget::screenToData(const QPoint& pos, const QRect& plotRect,
    int centerX, int centerY, int halfW, int halfH) const
{
    double dataHalfW = m_maxRange / m_zoom;
    double dataHalfH = m_maxRange / m_zoom;

    double nx = (static_cast<double>(pos.x()) - centerX) / halfW;
    double ny = (static_cast<double>(pos.y()) - centerY) / halfH;

    return QPointF(m_centerX + nx * dataHalfW,
        m_centerY - ny * dataHalfH);
}