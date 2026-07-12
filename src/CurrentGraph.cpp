// CustomCanvas.cpp
#include "CurrentGraph.h"

#include "OsdSettings.h"
#include "PowerDelivery.h"

#include <QDateTime>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QWheelEvent>
#include <cmath>

CurrentGraph::CurrentGraph(QWidget *parent, MeasurementPipeline *pipeline,
                           OsdSettings *settings)
    : QWidget(parent), pipeline(pipeline), settings(settings) {
  setMinimumSize(200, 150);
  setAttribute(Qt::WA_OpaquePaintEvent); // optional perf hint
  setFocusPolicy(Qt::StrongFocus);
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
    return ceil(max_current*2)/2;
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
  if (max_current >= .05) {
    return .1;
  }
  if (max_current >= .01) {
    return .05;
  }
  return ceil(max_current);
}

void CurrentGraph::setLive() {
  m_isLive = true;
  m_viewAnchorMs = 0;
  m_liveRightEdgeMs = 0;
  emit reviewModeChanged(false);
  update();
}

void CurrentGraph::panView(int directionMs) {
  if (m_isLive) {
    // Enter review mode anchored at the current newest frame.
    m_isLive = false;
    auto latest = pipeline->latestFrame();
    m_viewAnchorMs = latest.timestampMs;
    emit reviewModeChanged(true);
  }
  m_viewAnchorMs += directionMs;
  update();
}

void CurrentGraph::handleKey(QKeyEvent *event) {
  switch (event->key()) {
  case Qt::Key_Left:
    panView(-PanKeyStepMs);
    event->accept();
    return;
  case Qt::Key_Right:
    panView(+PanKeyStepMs);
    event->accept();
    return;
  case Qt::Key_Home:
    setLive();
    event->accept();
    return;
  default:
    break;
  }
  QWidget::keyPressEvent(event);
}

void CurrentGraph::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    setLive();
    event->accept();
    return;
  }
  QWidget::mousePressEvent(event);
}

void CurrentGraph::wheelEvent(QWheelEvent *event) {
  const int delta = event->angleDelta().y();
  if (delta != 0) {
    const int direction = delta > 0 ? -1 : +1;
    panView(direction * PanWheelStepMs);
    event->accept();
    return;
  }
  QWidget::wheelEvent(event);
}

void CurrentGraph::refresh() {
  update(); // schedules a paintEvent
}

namespace {
  // Maps a horizontal pixel index (0 = right/newest, w-1 = left/oldest) to a
  // time offset in seconds from the right edge for the stepped "log-like" view.
  // The newest 2/6 (1/3) uses linear resolution. The remaining 4/6 are divided
  // into four segments, each with half the resolution of the one before it:
  // 1x, 2x, 4x, 8x, 16x. So the leftmost segment has 16 measurements per pixel.
  // Leftover pixels from integer division are added to the leftmost segment.
  double steppedAgeForPixel(int i, int w, double pixelsPerSecond) {
    const double unit = 1.0 / pixelsPerSecond; // seconds per pixel at full resolution
    const int baseSegSize = w / 6;
    const int leftover = w - 6 * baseSegSize;

    const int segSizes[5] = {
        2 * baseSegSize,
        baseSegSize,
        baseSegSize,
        baseSegSize,
        baseSegSize + leftover,
    };
    const double multipliers[5] = {1.0, 2.0, 4.0, 8.0, 16.0};

    double age = 0.0;
    int consumed = 0;
    for (int seg = 0; seg < 5; ++seg) {
      const int segEnd = consumed + segSizes[seg];
      if (i <= segEnd) {
        return age + (i - consumed) * multipliers[seg] * unit;
      }
      age += segSizes[seg] * multipliers[seg] * unit;
      consumed = segEnd;
    }
    return age;
  }
}

void CurrentGraph::buildPixelMaps(
    double newestTime, double oldestTime,
    const std::vector<DisplayFrame> &frames,
    std::vector<double> &currentAtPixel,
    std::vector<double> &voltageAtPixel) const {
  const int w = width();
  const double pixelsPerSecond =
      std::max(settings->graph_pixels_per_second, 0.1);

  currentAtPixel.assign(w, 0.0);
  voltageAtPixel.assign(w, 0.0);
  std::vector<bool> hasPixel(w, false);

  // Convention: currentAtPixel[idx], idx = 0 is the left/oldest edge,
  // idx = w-1 is the right/newest edge.
  if (settings->graph_log_scale) {
    // Stepped "log-like" mapping: newest third at full resolution, then
    // resolution halves for each segment to the left. For every pixel column
    // we compute the [tLeft, tRight) window and take the max current/voltage.
    for (int i = 0; i < w; ++i) {
      const double dtLeft = steppedAgeForPixel(i, w, pixelsPerSecond);
      const double dtRight = steppedAgeForPixel(i + 1, w, pixelsPerSecond);
      const qint64 tRight = static_cast<qint64>(newestTime - dtLeft * 1000.0);
      const qint64 tLeft = static_cast<qint64>(newestTime - dtRight * 1000.0);

      double maxCurrent = 0.0;
      double voltage = 0.0;
      bool found = false;
      for (const auto &frame : frames) {
        if (frame.timestampMs <= tRight && frame.timestampMs > tLeft) {
          if (!found || frame.current > maxCurrent) {
            maxCurrent = frame.current;
            voltage = frame.voltage;
          }
          found = true;
        }
      }
      if (found) {
        // i is pixels from the right edge; x is the left-edge index.
        const int x = w - 1 - i;
        if (x >= 0 && x < w) {
          currentAtPixel[x] = maxCurrent;
          voltageAtPixel[x] = voltage;
          hasPixel[x] = true;
        }
      }
    }
  } else {
    // Linear mapping: one horizontal pixel = 1 / pixelsPerSecond seconds.
    // Newest data at the right edge (index w-1), oldest at the left (index 0).
    for (const auto &frame : frames) {
      if (frame.timestampMs > newestTime || frame.timestampMs < oldestTime) {
        continue;
      }
      const int x = w - 1 - static_cast<int>(
                                (newestTime - frame.timestampMs) / 1000.0 *
                                pixelsPerSecond);
      if (x >= 0 && x < w) {
        currentAtPixel[x] = frame.current;
        voltageAtPixel[x] = frame.voltage;
        hasPixel[x] = true;
      }
    }
  }

  // Fill small gaps *within* the data span by interpolating neighboring real
  // samples. Do not extend the trace into the empty left region before enough
  // history exists; those pixels are left at zero and the graph simply grows
  // from the right edge as a true strip chart.
  int firstRealIndex = w; // leftmost pixel with real data
  int lastRealIndex = -1; // rightmost pixel with real data
  for (int i = 0; i < w; ++i) {
    if (hasPixel[i]) {
      firstRealIndex = std::min(firstRealIndex, i);
      lastRealIndex = std::max(lastRealIndex, i);
    }
  }
  if (firstRealIndex < lastRealIndex) {
    for (int i = firstRealIndex + 1; i < lastRealIndex; ++i) {
      if (!hasPixel[i]) {
        currentAtPixel[i] = currentAtPixel[i + 1];
        voltageAtPixel[i] = voltageAtPixel[i + 1];
      }
    }
  }
}

void CurrentGraph::drawGrid(QPainter &p, double minCurrent,
                            double maxCurrent) {
  const int graphHeight = height() - 10;
  const int graphTop = 5;

  QPen pen(Qt::darkGray, 1);
  pen.setStyle(Qt::DotLine);
  p.setPen(pen);
  p.setBrush(Qt::NoBrush);
  p.setFont(QFont("Arial", 8));

  for (float yy : {10.0, 9.0, 8.0, 7.0, 6.0, 5.0, 4.0, 3.0, 2.0, 1.0,
                   0.5, 0.25, 0.1, 0.05, 0.0}) {
    if (yy >= minCurrent && yy <= maxCurrent) {
      const int y = graphTop + static_cast<int>(
                                  (maxCurrent - yy) / (maxCurrent - minCurrent) *
                                  graphHeight);
      p.drawLine(1, y, width() - 1, y);
      if (yy > 0.01)
        p.drawText(1, y - 1, QString("%1A").arg(yy, 0, 'f', 2));
    }
  }
}

void CurrentGraph::drawGraphLine(QPainter &p,
                                 const std::vector<double> &currentAtPixel,
                                 const std::vector<double> &voltageAtPixel,
                                 double minCurrent, double maxCurrent) {
  const int graphHeight = height() - 10;
  const int graphTop = 5;

  // Determine rightmost pixel that has real data so we don't draw a flat line
  // across the whole widget before enough history exists.
  int rightmostReal = -1;
  for (int i = currentAtPixel.size() - 1; i >= 0; --i) {
    if (voltageAtPixel[i] > 0.0 || currentAtPixel[i] > 0.0) {
      rightmostReal = i;
      break;
    }
  }
  if (rightmostReal < 0) {
    return;
  }

  QPointF lastPoint;
  bool hasLastPoint = false;

  for (int i = 0; i <= rightmostReal; ++i) {
    const double current = currentAtPixel[i];
    const double voltage = voltageAtPixel[i];

    auto pdVolts = PowerDelivery::getEnum(voltage);
    switch (pdVolts) {
    case PowerDelivery::PD_NONE:
      p.setPen(QPen(QColor(255, 128, 128), 1));
      break;
    case PowerDelivery::PD_5V:
      p.setPen(QPen(settings->color_5v, 1));
      break;
    case PowerDelivery::PD_9V:
      p.setPen(QPen(settings->color_9v, 1));
      break;
    case PowerDelivery::PD_15V:
      p.setPen(QPen(settings->color_15v, 1));
      break;
    case PowerDelivery::PD_20V:
      p.setPen(QPen(settings->color_20v, 1));
      break;
    case PowerDelivery::PD_28V:
      p.setPen(QPen(settings->color_28v, 1));
      break;
    case PowerDelivery::PD_36V:
      p.setPen(QPen(settings->color_36v, 1));
      break;
    case PowerDelivery::PD_48V:
      p.setPen(QPen(settings->color_48v, 1));
      break;
    }

    const int x = i; // left-to-right: oldest on left, newest on right
    const int y = graphTop + static_cast<int>(
                                (maxCurrent - current) /
                                (maxCurrent - minCurrent) * graphHeight);

    QPointF currentPoint(x, y);
    if (hasLastPoint) {
      p.drawLine(lastPoint, currentPoint);
    }
    lastPoint = currentPoint;
    hasLastPoint = true;
  }
}

void CurrentGraph::drawPeaks(QPainter &p,
                             const std::vector<double> &currentAtPixel,
                             double minCurrent, double maxCurrent) {
  if (!settings->show_graph_peaks) {
    return;
  }

  const int w = currentAtPixel.size();
  if (w == 0) {
    return;
  }

  // Find min/max values and the first pixel where each occurs.
  double peakMin = std::numeric_limits<double>::infinity();
  double peakMax = -std::numeric_limits<double>::infinity();
  int xMin = -1;
  int xMax = -1;
  for (int i = 0; i < w; ++i) {
    if (currentAtPixel[i] < peakMin) {
      peakMin = currentAtPixel[i];
      xMin = i;
    }
    if (currentAtPixel[i] > peakMax) {
      peakMax = currentAtPixel[i];
      xMax = i;
    }
  }

  if (xMin < 0 || xMax < 0 || !std::isfinite(peakMin) || !std::isfinite(peakMax)) {
    return;
  }

  const int graphHeight = height() - 10;
  const int graphTop = 5;
  const auto yForCurrent = [&](double current) {
    return graphTop + static_cast<int>(
                          (maxCurrent - current) /
                          (maxCurrent - minCurrent) * graphHeight);
  };

  QPen peakPen(Qt::cyan, 1, Qt::SolidLine);
  p.setPen(peakPen);
  p.setBrush(Qt::cyan);
  p.setFont(QFont("Arial", 9, QFont::Bold));

  const int yMax = yForCurrent(peakMax);
  const int yMin = yForCurrent(peakMin);

  // Small triangles pointing at the extrema.
  constexpr int triSize = 6;
  QPointF maxTriangle[] = {
      QPointF(xMax, yMax - triSize),
      QPointF(xMax - triSize, yMax - triSize - triSize),
      QPointF(xMax + triSize, yMax - triSize - triSize),
  };
  p.drawPolygon(maxTriangle, 3);
  p.drawText(xMax + triSize + 2, yMax - triSize,
             QString("%1A").arg(peakMax, 0, 'f', 3));

  QPointF minTriangle[] = {
      QPointF(xMin, yMin + triSize),
      QPointF(xMin - triSize, yMin + triSize + triSize),
      QPointF(xMin + triSize, yMin + triSize + triSize),
  };
  p.drawPolygon(minTriangle, 3);
  p.drawText(xMin + triSize + 2, yMin + triSize + triSize,
             QString("%1A").arg(peakMin, 0, 'f', 3));
}

void CurrentGraph::drawReviewBorder(QPainter &p) {
  if (m_isLive) {
    return;
  }
  QPen borderPen(QColor(255, 80, 80), 3);
  p.setPen(borderPen);
  p.setBrush(Qt::NoBrush);
  p.drawRect(rect().adjusted(1, 1, -1, -1));
}

void CurrentGraph::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);

  // Background
  p.fillRect(rect(), QColor(30, 30, 30));

  // Only draw if we have data
  if (!pipeline || pipeline->lastNFrames(1).empty()) {
    p.setPen(Qt::white);
    p.drawText(rect(), Qt::AlignCenter, "No Data");
    return;
  }

  const int w = width();
  const double pixelsPerSecond = std::max(settings->graph_pixels_per_second, 0.1);

  if (!pipeline || pipeline->lastNFrames(1).empty()) {
    return;
  }

  const double maxAgeSeconds = settings->graph_log_scale
                                  ? steppedAgeForPixel(w, w, pixelsPerSecond)
                                  : static_cast<double>(w) / pixelsPerSecond;

  const auto latestFrame = pipeline->latestFrame();
  const qint64 newestTime = latestFrame.timestampMs;
  qint64 viewRightMs;
  if (m_isLive) {
    if (pipeline->isPaused()) {
      // Freeze the live view at the last data point when paused.
      if (m_liveRightEdgeMs == 0) {
        m_liveRightEdgeMs = newestTime;
      }
      viewRightMs = m_liveRightEdgeMs;
    } else {
      // Quantize the live right edge to whole pixel columns. This prevents
      // sub-pixel aliasing that makes the historic trace appear to jump up/down
      // while the graph is continuously scrolling.
      const qint64 msPerPixel =
          static_cast<qint64>(std::round(1000.0 / pixelsPerSecond));
      if (m_liveRightEdgeMs == 0 || newestTime >= m_liveRightEdgeMs + msPerPixel) {
        m_liveRightEdgeMs = newestTime - (newestTime % msPerPixel);
      }
      viewRightMs = m_liveRightEdgeMs;
    }
  } else {
    viewRightMs = m_viewAnchorMs;
  }

  // Use a fixed visible time window so the X-axis does not jump while the
  // strip chart is still filling. In live mode the newest sample stays at the
  // right edge and the trace grows leftward; once it reaches the left edge it
  // scrolls. Review mode pans the fixed window.
  const qint64 viewLeftMs =
      viewRightMs - static_cast<qint64>(maxAgeSeconds * 1000.0);

  // Query frames by the exact displayed time range, not by wall-clock now.
  std::vector<DisplayFrame> visibleFrames =
      pipeline->framesInRange(viewLeftMs, viewRightMs);
  if (visibleFrames.empty()) {
    visibleFrames.push_back(latestFrame);
  }

  const qint64 effectiveNewest = viewRightMs;
  const qint64 effectiveOldest = viewLeftMs;

  std::vector<double> currentAtPixel;
  std::vector<double> voltageAtPixel;
  buildPixelMaps(effectiveNewest, effectiveOldest, visibleFrames,
                 currentAtPixel, voltageAtPixel);

  // Find min/max over the actual visible frames, not over the entire history
  // or over the raw sample window. This mirrors the visible graph trace.
  bool haveData = false;
  double minCurrent = std::numeric_limits<double>::max();
  double maxCurrent = std::numeric_limits<double>::lowest();
  for (const auto &frame : visibleFrames) {
    if (frame.voltage > 0.0 || frame.current > 0.0) {
      haveData = true;
      minCurrent = std::min(minCurrent, frame.current);
      maxCurrent = std::max(maxCurrent, frame.current);
    }
  }
  if (!haveData) {
    minCurrent = 0.0;
    maxCurrent = 0.01;
  }

  // Current is never negative; anchor the bottom of the graph at 0.
  minCurrent = 0.0;
  maxCurrent = findHighBox(maxCurrent);

  // Add a little headroom above the maximum.
  const double range = maxCurrent - minCurrent;
  maxCurrent += range * 0.1;
  if (maxCurrent <= minCurrent || maxCurrent <= 0.0) {
    maxCurrent = minCurrent + 0.01;
  }

  drawGrid(p, minCurrent, maxCurrent);

  drawGraphLine(p, currentAtPixel, voltageAtPixel, minCurrent, maxCurrent);
  drawPeaks(p, currentAtPixel, minCurrent, maxCurrent);
  drawReviewBorder(p);

  if (pipeline && pipeline->isPaused()) {
    p.setPen(Qt::white);
    p.setFont(QFont("Arial", 14, QFont::Bold));
    p.drawText(rect(), Qt::AlignCenter, "PAUSED");
  }
}
