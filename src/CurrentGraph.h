#pragma once

#include "GraphCache.h"
#include "MeasurementPipeline.h"
#include "OsdSettings.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QTimer>
#include <QWheelEvent>
#include <QWidget>

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
 *
 * Internally the graph uses a GraphCache that holds one entry per horizontal
 * pixel, ordered newest-first. In live mode only the newest entries are
 * updated incrementally; in review/history mode the whole cache is rebuilt
 * (linear only).
 */
class CurrentGraph : public QWidget {
  Q_OBJECT
public:
  explicit CurrentGraph(QWidget *parent, MeasurementPipeline *pipeline,
                        OsdSettings *settings);

  void setLive();
  [[nodiscard]] bool isLive() const { return m_isLive; }
  void panView(int directionMs);
  void refresh();

  /**
   * @brief Mark the graph data cache as invalid.
   *
   * Call this when the time-to-pixel mapping changes (log/linear toggle,
   * resize, pan, live/review switch). The next paint will rebuild the cache.
   */
  void invalidateCache();

  [[nodiscard]] double visibleMinCurrent() const { return m_visibleMinCurrent; }
  [[nodiscard]] double visibleMaxCurrent() const { return m_visibleMaxCurrent; }
  [[nodiscard]] bool hasVisibleData() const { return m_hasVisibleData; }

  /**
   * @brief Stable mapping from frames to per-pixel columns.
   *
   * Exposed for unit tests. Output vectors are ordered left-to-right
   * (index 0 = oldest pixel, index width-1 = newest pixel).
   */
  static void buildPixelMaps(int width, bool logScale, double pixelsPerSecond,
                               double newestTime, double oldestTime,
                               const std::vector<DisplayFrame> &frames,
                               std::vector<double> &currentAtPixel,
                               std::vector<double> &voltageAtPixel);

signals:
  void reviewModeChanged(bool reviewing);

public:
  void handleKey(QKeyEvent *event);

protected:
  void resizeEvent(QResizeEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override { handleKey(event); }
  void mousePressEvent(QMouseEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;
  void paintEvent(QPaintEvent *event) override;

private:
  double findLowBox(double min_current);
  double findHighBox(double max_current);

  [[nodiscard]] double msPerPixel() const;
  [[nodiscard]] qint64 snapRightEdge(qint64 newestTime) const;

  void drawGrid(QPainter &p, double minCurrent, double maxCurrent);
  void drawGraphLine(QPainter &p, double minCurrent, double maxCurrent,
                     double pps, double actualMaxAgeSeconds);
  void drawPeaks(QPainter &p, double minCurrent, double maxCurrent,
                 double pps, double actualMaxAgeSeconds);
  void drawReviewBorder(QPainter &p);

  MeasurementPipeline *pipeline;
  OsdSettings *settings;

  GraphCache m_cache;

  bool m_isLive = true;
  qint64 m_viewAnchorMs = 0;       // right edge when not live
  qint64 m_liveRightEdgeMs = 0;    // right edge snapped to pixel grid
  static constexpr int PanKeyStepMs = 1000;
  static constexpr int PanWheelStepMs = 5000;

  double m_visibleMinCurrent = 0.0;
  double m_visibleMaxCurrent = 0.0;
  bool m_hasVisibleData = false;
};
