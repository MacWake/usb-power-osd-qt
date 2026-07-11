#pragma once
#include "MeasurementPipeline.h"
#include "OsdSettings.h"

#include <QTimer>
#include <QWidget>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>

/**
 * @brief Time-based current-history graph.
 *
 * The X-axis represents a configurable duration of wall-clock time
 * (pixels-per-second from OsdSettings). The graph draws the newest data on
 * the right and interpolates for missing pixel columns.
 *
 * Optional logarithmic time scale gives the most recent data the highest
 * horizontal resolution. The user can also pan through historic data with
 * cursor keys and the mouse wheel; a single click returns to live mode.
 */
class CurrentGraph : public QWidget {
    Q_OBJECT
public:
    explicit CurrentGraph(QWidget* parent, MeasurementPipeline *pipeline, OsdSettings *settings);

    void setLive();
    [[nodiscard]] bool isLive() const { return m_isLive; }
    void panView(int directionMs);
    void refresh();

signals:
    void reviewModeChanged(bool reviewing);

public:
    void handleKey(QKeyEvent *event);

protected:
    void keyPressEvent(QKeyEvent *event) override { handleKey(event); }
    void mousePressEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    double findLowBox(double min_current);
    double findHighBox(double max_current);

    void buildPixelMaps(double newestTime, double oldestTime,
                        const std::vector<DisplayFrame> &frames,
                        std::vector<double> &currentAtPixel,
                        std::vector<double> &voltageAtPixel) const;
    void drawGrid(QPainter &p, double minCurrent, double maxCurrent);
    void drawGraphLine(QPainter &p, const std::vector<double> &currentAtPixel,
                       const std::vector<double> &voltageAtPixel,
                       double minCurrent, double maxCurrent);
    void drawPeaks(QPainter &p, const std::vector<double> &currentAtPixel,
                   double minCurrent, double maxCurrent);
    void drawReviewBorder(QPainter &p);

    MeasurementPipeline *pipeline;
    OsdSettings *settings;

    bool m_isLive = true;
    qint64 m_viewAnchorMs = 0; // right edge when not live
    qint64 m_liveRightEdgeMs = 0; // right edge quantized to whole pixels
    static constexpr int PanKeyStepMs = 1000;
    static constexpr int PanWheelStepMs = 5000;
};
