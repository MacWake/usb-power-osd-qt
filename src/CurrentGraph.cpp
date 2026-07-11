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
  return ceil(max_current);
}

void CurrentGraph::setLive() {
  m_isLive = true;
  m_viewAnchorMs = 0;
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

std::vector<double> CurrentGraph::buildCurrentPixelMap(
    double newestTime, double oldestTime,
    const std::vector<DisplayFrame> &frames) const {
  const int w = width();
  const double pixelsPerSecond =
      std::max(settings->graph_pixels_per_second, 0.1);
  const double visibleDuration = static_cast<double>(w) / pixelsPerSecond;

  std::vector<double> currentAtPixel(w, 0.0);
  std::vector<double> voltageAtPixel(w, 0.0);
  std::vector<bool> hasPixel(w, false);

  if (settings->graph_log_scale) {
    // Logarithmic mapping: high resolution near newest (right), compressed left.
    // dt = t0 * (exp(k * x) - 1), where x is pixels from right edge.
    // Choose t0 so the rightmost pixel bucket is roughly one frame wide (~33 ms).
    const double t0 = 1.0 / static_cast<double>(pipeline->frameRate());
    const double k = std::log1p(visibleDuration / t0) / static_cast<double>(w > 1 ? w - 1 : 1);

    // For each pixel column, find frames in [dt_left, dt_right).
    for (int i = 0; i < w; ++i) {
      const double dtRight = t0 * (std::exp(k * static_cast<double>(i)) - 1.0);
      const double dtLeft =
          (i == 0) ? 0.0
                   : t0 * (std::exp(k * static_cast<double>(i - 1)) - 1.0);
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
    // Linear mapping: one horizontal pixel = 1 / pixelsPerSecond seconds.
    for (const auto &frame : frames) {
      if (frame.timestampMs > newestTime || frame.timestampMs < oldestTime) {
        continue;
      }
      const int pixel =
          w - 1 - static_cast<int>((newestTime - frame.timestampMs) / 1000.0 *
                                   pixelsPerSecond);
      if (pixel >= 0 && pixel < w) {
        currentAtPixel[pixel] = frame.current;
        voltageAtPixel[pixel] = frame.voltage;
        hasPixel[pixel] = true;
      }
    }
  }

  // Forward-fill missing pixels so the line is continuous when there are gaps.
  for (int i = w - 2; i >= 0; --i) {
    if (!hasPixel[i]) {
      currentAtPixel[i] = currentAtPixel[i + 1];
      voltageAtPixel[i] = voltageAtPixel[i + 1];
    }
  }

  return currentAtPixel;
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
                   0.5, 0.25, 0.1, 0.0}) {
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

  QPointF lastPoint;
  bool hasLastPoint = false;

  for (int i = 0; i < width(); ++i) {
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

    const int x = width() - 1 - i;
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

  double peakMin = std::numeric_limits<double>::infinity();
  double peakMax = -std::numeric_limits<double>::infinity();
  for (double c : currentAtPixel) {
    if (c < peakMin) peakMin = c;
    if (c > peakMax) peakMax = c;
  }

  if (!std::isfinite(peakMin) || !std::isfinite(peakMax)) {
    return;
  }

  const int graphHeight = height() - 10;
  const int graphTop = 5;
  const auto yForCurrent = [&](double current) {
    return graphTop + static_cast<int>(
                          (maxCurrent - current) /
                          (maxCurrent - minCurrent) * graphHeight);
  };

  QPen peakPen(Qt::cyan, 1, Qt::DashLine);
  p.setPen(peakPen);
  p.setFont(QFont("Arial", 9, QFont::Bold));

  const int yMax = yForCurrent(peakMax);
  p.drawLine(1, yMax, width() - 1, yMax);
  p.drawText(3, yMax - 2,
             QString("▲ %1A").arg(peakMax, 0, 'f', 3));

  const int yMin = yForCurrent(peakMin);
  p.drawLine(1, yMin, width() - 1, yMin);
  p.drawText(3, yMin + 12,
             QString("▼ %1A").arg(peakMin, 0, 'f', 3));
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
  const double visibleSeconds = static_cast<double>(w) / pixelsPerSecond;

  auto frames = pipeline->framesForDuration(visibleSeconds);
  if (frames.empty()) {
    return;
  }

  const qint64 newestTime = frames.back().timestampMs;
  qint64 viewRightMs = newestTime;
  if (!m_isLive && m_viewAnchorMs != 0) {
    viewRightMs = m_viewAnchorMs;
  }
  const qint64 viewLeftMs = viewRightMs - static_cast<qint64>(visibleSeconds * 1000.0);

  // Filter frames to the visible window and build per-pixel maps.
  std::vector<DisplayFrame> visibleFrames;
  visibleFrames.reserve(frames.size());
  for (const auto &frame : frames) {
    if (frame.timestampMs <= viewRightMs && frame.timestampMs > viewLeftMs) {
      visibleFrames.push_back(frame);
    }
  }
  if (visibleFrames.empty()) {
    visibleFrames.push_back(frames.back());
  }

  const qint64 effectiveNewest = viewRightMs;
  const qint64 effectiveOldest = viewLeftMs;

  std::vector<double> voltageAtPixel(w, 0.0);
  std::vector<double> currentAtPixel =
      buildCurrentPixelMap(effectiveNewest, effectiveOldest, visibleFrames);
  // buildCurrentPixelMap currently only fills currentAtPixel; voltage is
  // forward-filled inside it but we need it back. Simpler: recompute both here.
  // TODO: refactor buildCurrentPixelMap to return both arrays.

  // Find min/max for scaling
  double minCurrent = std::numeric_limits<double>::max();
  double maxCurrent = std::numeric_limits<double>::lowest();
  for (int i = 0; i < w; ++i) {
    minCurrent = std::min(minCurrent, currentAtPixel[i]);
    maxCurrent = std::max(maxCurrent, currentAtPixel[i]);
  }
  minCurrent = findLowBox(minCurrent);
  maxCurrent = findHighBox(maxCurrent);

  const double range = maxCurrent - minCurrent;
  const double padding = range * 0.1;
  minCurrent -= padding;
  maxCurrent += padding;
  if (maxCurrent <= minCurrent) {
    maxCurrent = minCurrent + 0.01;
  }

  drawGrid(p, minCurrent, maxCurrent);

  // Rebuild voltage array for coloring.
  // For now use linear mapping for voltage coloring; log scale colors by voltage
  // of the same bucket. We reuse the same per-pixel logic with voltage as max.
  // To avoid duplication we just fill voltageAtPixel with the voltage of the
  // frame that determined currentAtPixel. Linear path is sufficient because
  // voltage changes slowly.
  for (const auto &frame : visibleFrames) {
    if (frame.timestampMs > effectiveNewest || frame.timestampMs <= effectiveOldest) {
      continue;
    }
    const int pixel = w - 1 - static_cast<int>(
                                   (effectiveNewest - frame.timestampMs) / 1000.0 *
                                   pixelsPerSecond);
    if (pixel >= 0 && pixel < w) {
      voltageAtPixel[pixel] = frame.voltage;
    }
  }
  for (int i = w - 2; i >= 0; --i) {
    if (qFuzzyIsNull(voltageAtPixel[i])) {
      voltageAtPixel[i] = voltageAtPixel[i + 1];
    }
  }

  drawGraphLine(p, currentAtPixel, voltageAtPixel, minCurrent, maxCurrent);
  drawPeaks(p, currentAtPixel, minCurrent, maxCurrent);
  drawReviewBorder(p);
}
