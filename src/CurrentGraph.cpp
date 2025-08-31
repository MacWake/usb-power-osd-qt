// CustomCanvas.cpp
#include "CurrentGraph.h"

#include "OsdSettings.h"
#include "PowerDelivery.h"

#include <QPaintEvent>
#include <QPainter>

CurrentGraph::CurrentGraph(QWidget* parent, MeasurementHistory *history, OsdSettings *settings)
    : QWidget(parent), history(history), settings(settings), m_refreshTimer(new QTimer(this))
{
    setMinimumSize(200, 150);
    setAttribute(Qt::WA_OpaquePaintEvent); // optional perf hint

    // Setup the refresh timer
    connect(m_refreshTimer, &QTimer::timeout, this, &CurrentGraph::updateGraph);

    m_refreshTimer->start(50);

}
double CurrentGraph::findLowBox(double min_current) {
    if (min_current < .1) {
        return 0.0;
    }
    if (min_current < .25) {
      return .1;
    }
    if (min_current < .5) {
      return .25;
    }
    if (min_current < 1.0) {
      return .5;
    }
    return floor(min_current);
}
double CurrentGraph::findHighBox(double max_current) {
    if (max_current >= 1.0) {
        return ceil(max_current);
    }
    if (max_current >= .5) {
        return 1.0;
    }
    if (max_current >= .25) {
        return .5;
    }
    if (max_current >= .1) {
        return .25;
    }
    if (max_current >= .01) {
        return .1;
    }
}

void CurrentGraph::updateGraph() {
    // This slot is called by the timer to trigger a repaint
    update(); // This schedules a paintEvent
}

void CurrentGraph::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Background
    p.fillRect(rect(), QColor(30, 30, 30));

    // Only draw if we have data
    if (!history || history->is_empty()) {
        p.setPen(Qt::white);
        p.drawText(rect(), Qt::AlignCenter, "No Data");
        return;
    }

    // Get recent samples for smooth animation
    const int maxSamples = width(); // One sample per pixel width
    auto samples = history->lastNSamplesNewestFirst(maxSamples);

    if (samples.empty()) return;

    // Find min/max for scaling
    double minCurrent = std::numeric_limits<double>::max();
    double maxCurrent = std::numeric_limits<double>::lowest();

    for (const auto& sample : samples) {
        minCurrent = std::min(minCurrent, sample.current);
        maxCurrent = std::max(maxCurrent, sample.current);
    }
    minCurrent = findLowBox(minCurrent);
    maxCurrent = findHighBox(maxCurrent);

    // Add some padding to the range
    const double range = maxCurrent - minCurrent;
    const double padding = range * 0.1;
    minCurrent -= padding;
    maxCurrent += padding;

    if (maxCurrent <= minCurrent) {
        maxCurrent = minCurrent + 0.01; // Avoid division by zero
    }
    const int graphHeight = height() - 10; // Leave margin for labels
    const int graphTop = 5;

    QPen pen(Qt::darkGray, 1);
    pen.setStyle(Qt::DotLine);
    // Draw grid
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.setFont(QFont("Arial", 8));

    for (float yy: {10.0,9.0,8.0,7.0,6.0,5.0,4.0,3.0,2.0,1.0,0.5,0.25,0.1,0.0}) {
        if (yy >= minCurrent && yy <= maxCurrent) {
            const int y = graphTop + (int)((maxCurrent - yy) / (maxCurrent - minCurrent) * graphHeight);

            p.drawLine(1, y, width()-1, y);
            if (yy > 0.01)p.drawText(1, y-1, QString("%1A").arg(yy, 0, 'f', 2));

        }
    }

    // Draw the current graph

    QPointF lastPoint;
    bool hasLastPoint = false;

    for (int i = 0; i < samples.size() && i < width(); ++i) {
        const double current = samples[i].current;

        const double voltage = samples[i].voltage;
        auto pdVolts = PowerDelivery::getEnum(voltage);
        switch (pdVolts) {
            case PowerDelivery::PD_NONE:
                p.setPen(QPen(QColor(255,128,128), 1)); // Red line
                break;
            case PowerDelivery::PD_5V:
                p.setPen(QPen(settings->color_5v, 1)); // Green line
                break;
            case PowerDelivery::PD_9V:
                p.setPen(QPen(settings->color_9v, 1)); // Blue line
                break;
            case PowerDelivery::PD_15V:
            p.setPen(QPen(settings->color_15v, 1)); // Yellow line
            break;
            case PowerDelivery::PD_20V:
            p.setPen(QPen(settings->color_20v, 1)); // Orange line
            break;
            case PowerDelivery::PD_28V:
            p.setPen(QPen(settings->color_28v, 1)); // Purple line
            break;
            case PowerDelivery::PD_36V:
            p.setPen(QPen(settings->color_36v, 1)); // Pink line
            break;
            case PowerDelivery::PD_48V:
            p.setPen(QPen(settings->color_48v, 1)); // Brown line
        }
        //p.setPen(QPen(QColor(0, 255, 128), 1)); // Bright green line

        // Scale to widget coordinates
        const int x = width() - 1 - i; // Newest on right, oldest on left
        const int y = graphTop + (int)((maxCurrent - current) / (maxCurrent - minCurrent) * graphHeight);

        QPointF currentPoint(x, y);

        if (hasLastPoint) {
            p.drawLine(lastPoint, currentPoint);
        }

        lastPoint = currentPoint;
        hasLastPoint = true;
    }

    // Draw scale labels
    p.setPen(Qt::white);


    // // Outline
    // p.setPen(QPen(Qt::white, 1));
    // p.setBrush(Qt::NoBrush);
    // p.drawRect(rect().adjusted(0, 0, -1, -1));
}
