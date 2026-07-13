// CurrentGraph.cpp
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

namespace {

// Maps a horizontal pixel index (0 = right/newest, w-1 = left/oldest) to a
// time offset in seconds from the right edge for the stepped "log-like" view.
// The newest 2/6 (1/3) uses linear resolution. The remaining 4/6 are divided
// into four segments, each with half the resolution of the one before it:
// 1x, 2x, 4x, 8x, 16x. Leftover pixels from integer division are added to the
// leftmost segment.
double steppedAgeForPixel(int idx, int w, double pixelsPerSecond) {
  const double unit = 1.0 / std::max(pixelsPerSecond, 0.1);
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
    if (idx <= segEnd) {
      return age + (idx - consumed) * multipliers[seg] * unit;
    }
    age += segSizes[seg] * multipliers[seg] * unit;
    consumed = segEnd;
  }
  return age;
}

} // namespace

CurrentGraph::CurrentGraph(QWidget *parent, MeasurementPipeline *pipeline,
                           OsdSettings *settings)
    : QWidget(parent), pipeline(pipeline), settings(settings) {
  setMinimumSize(200, 150);
  setAttribute(Qt::WA_OpaquePaintEvent); // optional perf hint
  setFocusPolicy(Qt::StrongFocus);
  m_cache.setSize(width());
  m_cache.setParams(settings->graph_log_scale,
                    settings->graph_pixels_per_second, true);
}

double CurrentGraph::msPerPixel() const {
  return 1000.0 / std::max(settings->graph_pixels_per_second, 0.1);
}

qint64 CurrentGraph::snapRightEdge(qint64 newestTime) const {
  // Snap the live right edge up to the next pixel-grid boundary so the newest
  // sample always falls inside the rightmost pixel column.
  const double unit = msPerPixel();
  return static_cast<qint64>(
      std::ceil(static_cast<double>(newestTime) / unit) * unit);
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
    return ceil(max_current * 2) / 2;
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
  m_cache.setParams(settings->graph_log_scale,
                    settings->graph_pixels_per_second, true);
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
  // History/review mode is linear per the graph cache spec.
  m_cache.setParams(false, settings->graph_pixels_per_second, false);
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

void CurrentGraph::invalidateCache() {
  const bool effectiveLogScale = m_isLive && settings->graph_log_scale;
  m_cache.setParams(effectiveLogScale, settings->graph_pixels_per_second,
                    m_isLive);
}

void CurrentGraph::resizeEvent(QResizeEvent *event) {
  Q_UNUSED(event);
  m_cache.setSize(width());
  invalidateCache();
  update();
}

void CurrentGraph::buildPixelMaps(
    int w, bool logScale, double pixelsPerSecond, double newestTime,
    double oldestTime, const std::vector<DisplayFrame> &frames,
    std::vector<double> &currentAtPixel,
    std::vector<double> &voltageAtPixel) {
  currentAtPixel.assign(w, 0.0);
  voltageAtPixel.assign(w, 0.0);
  std::vector<bool> hasPixel(w, false);

  if (logScale) {
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
        const int x = w - 1 - i;
        if (x >= 0 && x < w) {
          currentAtPixel[x] = maxCurrent;
          voltageAtPixel[x] = voltage;
          hasPixel[x] = true;
        }
      }
    }
  } else {
    const double msPx = 1000.0 / std::max(pixelsPerSecond, 0.1);
    for (const auto &frame : frames) {
      const int i = static_cast<int>(
          std::floor((newestTime - frame.timestampMs) / msPx));
      if (i < 0 || i >= w) {
        continue;
      }
      const int x = w - 1 - i;
      if (x >= 0 && x < w) {
        currentAtPixel[x] = frame.current;
        voltageAtPixel[x] = frame.voltage;
        hasPixel[x] = true;
      }
    }
  }

  // Forward-fill small gaps within the data span.
  int firstRealIndex = w;
  int lastRealIndex = -1;
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

void CurrentGraph::drawGraphLine(QPainter &p, double minCurrent,
                                 double maxCurrent, double pps,
                                 double actualMaxAgeSeconds) {
  const int graphHeight = height() - 10;
  const int graphTop = 5;
  const int w = m_cache.size();
  const auto &entries = m_cache.entries();
  const bool logScale = m_cache.logScale();
  const double unitMs = 1000.0 / std::max(pps, 0.1);

  QPointF lastPoint;
  bool hasLastPoint = false;

  for (int x = 0; x < w; ++x) {
    const int cacheIdx = w - 1 - x;
    const auto &entry = entries[cacheIdx];
    if (entry.voltage < 0.0) {
      hasLastPoint = false;
      continue;
    }

    // Don't draw pixels that represent an age beyond the actual data span.
    // In log mode old pixels are derived from newer data and would otherwise
    // make it look like history exists before the device started sending data.
    const double representedAgeSeconds =
        logScale ? steppedAgeForPixel(cacheIdx, w, pps)
                 : static_cast<double>(cacheIdx) * unitMs / 1000.0;
    if (representedAgeSeconds > actualMaxAgeSeconds + 1e-9) {
      hasLastPoint = false;
      continue;
    }

    auto pdVolts = PowerDelivery::getEnum(entry.voltage);
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

    const int y = graphTop + static_cast<int>(
                                (maxCurrent - entry.current) /
                                (maxCurrent - minCurrent) * graphHeight);

    QPointF currentPoint(x, y);
    if (hasLastPoint) {
      p.drawLine(lastPoint, currentPoint);
    }
    lastPoint = currentPoint;
    hasLastPoint = true;
  }
}

void CurrentGraph::drawPeaks(QPainter &p, double minCurrent,
                             double maxCurrent, double pps,
                             double actualMaxAgeSeconds) {
  if (!settings->show_graph_peaks) {
    return;
  }

  const int w = m_cache.size();
  if (w == 0) {
    return;
  }
  const auto &entries = m_cache.entries();
  const bool logScale = m_cache.logScale();
  const double unitMs = 1000.0 / std::max(pps, 0.1);

  double peakMin = std::numeric_limits<double>::infinity();
  double peakMax = -std::numeric_limits<double>::infinity();
  int xMin = -1;
  int xMax = -1;
  for (int cacheIdx = 0; cacheIdx < w; ++cacheIdx) {
    const auto &entry = entries[cacheIdx];
    if (entry.voltage < 0.0) {
      continue;
    }

    const double ageSeconds =
        logScale ? steppedAgeForPixel(cacheIdx, w, pps)
                 : static_cast<double>(cacheIdx) * unitMs / 1000.0;
    if (ageSeconds > actualMaxAgeSeconds + 1e-9) {
      continue;
    }

    const int x = w - 1 - cacheIdx;
    if (entry.current < peakMin) {
      peakMin = entry.current;
      xMin = x;
    }
    if (entry.current > peakMax) {
      peakMax = entry.current;
      xMax = x;
    }
  }

  if (xMin < 0 || xMax < 0 || !std::isfinite(peakMin) ||
      !std::isfinite(peakMax)) {
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
  const double pixelsPerSecond =
      std::max(settings->graph_pixels_per_second, 0.1);

  const double maxAgeSeconds =
      settings->graph_log_scale
          ? steppedAgeForPixel(w, w, pixelsPerSecond)
          : static_cast<double>(w) / pixelsPerSecond;

  const auto latestFrame = pipeline->latestFrame();
  const qint64 newestTime = latestFrame.timestampMs;
  qint64 viewRightMs;
  if (m_isLive) {
    if (pipeline->isPaused()) {
      if (m_liveRightEdgeMs == 0) {
        m_liveRightEdgeMs = snapRightEdge(newestTime);
      }
      viewRightMs = m_liveRightEdgeMs;
    } else {
      viewRightMs = snapRightEdge(newestTime);
      m_liveRightEdgeMs = viewRightMs;
    }
  } else {
    viewRightMs = m_viewAnchorMs;
  }

  const qint64 viewLeftMs =
      viewRightMs - static_cast<qint64>(maxAgeSeconds * 1000.0);

  m_cache.setSize(w);
  // History/review mode is linear per spec; live mode respects the setting.
  const bool effectiveLogScale = m_isLive && settings->graph_log_scale;
  m_cache.setParams(effectiveLogScale, pixelsPerSecond, m_isLive);

  const auto frames = pipeline->framesInRange(viewLeftMs, viewRightMs);
  qint64 actualDataLeftMs = viewRightMs;
  if (!frames.empty()) {
    actualDataLeftMs = frames.front().timestampMs;
  }
  const double actualMaxAgeSeconds =
      static_cast<double>(viewRightMs - actualDataLeftMs) / 1000.0;

  if (m_isLive) {
    m_cache.updateLive(frames, viewRightMs);
  } else {
    m_cache.rebuild(frames, viewRightMs);
  }

  // Compute visible min/max only from entries whose represented age is within
  // the actual data span. Otherwise log-mode derived pixels at the far left make
  // it look like history exists before the device started sending data.
  const auto &entries = m_cache.entries();
  const double unitMs = 1000.0 / pixelsPerSecond;
  bool haveVisibleData = false;
  double visibleMin = std::numeric_limits<double>::max();
  double visibleMax = std::numeric_limits<double>::lowest();
  for (int i = 0; i < w; ++i) {
    if (entries[i].voltage < 0.0) {
      continue;
    }
    const double ageSeconds =
        effectiveLogScale
            ? steppedAgeForPixel(i, w, pixelsPerSecond)
            : static_cast<double>(i) * unitMs / 1000.0;
    if (ageSeconds > actualMaxAgeSeconds + 1e-9) {
      continue;
    }
    haveVisibleData = true;
    visibleMin = std::min(visibleMin, entries[i].current);
    visibleMax = std::max(visibleMax, entries[i].current);
  }
  m_hasVisibleData = haveVisibleData;
  m_visibleMinCurrent = haveVisibleData ? visibleMin : 0.0;
  m_visibleMaxCurrent = haveVisibleData ? visibleMax : 0.0;

  bool haveData = m_hasVisibleData;
  double maxCurrent = m_visibleMaxCurrent;

  if (!haveData) {
    maxCurrent = 0.01;
  }

  // Current is never negative; anchor the bottom of the graph at 0.
  const double minCurrent = 0.0;
  maxCurrent = findHighBox(maxCurrent);

  // Add a little headroom above the maximum.
  const double range = maxCurrent - minCurrent;
  maxCurrent += range * 0.1;
  if (maxCurrent <= minCurrent || maxCurrent <= 0.0) {
    maxCurrent = minCurrent + 0.01;
  }

  drawGrid(p, minCurrent, maxCurrent);

  drawGraphLine(p, minCurrent, maxCurrent, pixelsPerSecond,
                actualMaxAgeSeconds);
  drawPeaks(p, minCurrent, maxCurrent, pixelsPerSecond,
            actualMaxAgeSeconds);
  drawReviewBorder(p);

  if (pipeline && pipeline->isPaused()) {
    p.setPen(Qt::white);
    p.setFont(QFont("Arial", 14, QFont::Bold));
    p.drawText(rect(), Qt::AlignCenter, "PAUSED");
  }
}
