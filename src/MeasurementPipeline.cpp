// MeasurementPipeline.cpp
#include "MeasurementPipeline.h"

#include <QDateTime>
#include <algorithm>
#include <cmath>
#include <limits>

MeasurementPipeline::MeasurementPipeline(QObject *parent)
    : QObject(parent), m_frameTimer(new QTimer(this)) {
  m_frameTimer->setTimerType(Qt::PreciseTimer);
  connect(m_frameTimer, &QTimer::timeout, this, &MeasurementPipeline::onFrameTimer);
  setFrameRate(DefaultFrameRate);
  m_frameTimer->start();

  // Default: one hour of 30 Hz frames gives 4K@1px/s (≈64 min visible) with review headroom.
  setMaxHistoryDuration(DefaultHistorySeconds);
}

void MeasurementPipeline::pushSample(const PowerData &sample) {
  {
    std::lock_guard<std::mutex> lock(m_rawMutex);
    m_rawWindow.push_back(sample);
    while (m_rawWindow.size() > MaxRawWindowSize) {
      m_rawWindow.pop_front();
    }
  }

  {
    std::lock_guard<std::mutex> lock(m_frameMutex);
    if (!m_bucketHasData.exchange(true)) {
      m_currentBucket.timestampMs = sample.timestamp;
      m_currentBucket.voltage = sample.voltage;
      m_currentBucket.current = sample.current;
      m_currentBucket.power = sample.power;
      m_currentBucket.energyWh = sample.energy;
      m_currentBucket.sampleCount = 1;
    } else {
      m_currentBucket.timestampMs = sample.timestamp;
      m_currentBucket.current = std::max(m_currentBucket.current, sample.current);
      m_currentBucket.power = std::max(m_currentBucket.power, sample.power);
      // Voltage and energy change slowly; keep latest.
      m_currentBucket.voltage = sample.voltage;
      m_currentBucket.energyWh = sample.energy;
      ++m_currentBucket.sampleCount;
    }
  }
}

void MeasurementPipeline::reset() {
  {
    std::lock_guard<std::mutex> lock(m_rawMutex);
    m_rawWindow.clear();
  }
  {
    std::lock_guard<std::mutex> lock(m_frameMutex);
    m_frames.clear();
    m_bucketHasData = false;
    m_currentBucket = DisplayFrame{};
  }
}

DisplayFrame MeasurementPipeline::latestFrame() const {
  std::lock_guard<std::mutex> lock(m_frameMutex);
  if (!m_frames.empty()) {
    return m_frames.back();
  }
  if (m_bucketHasData) {
    return m_currentBucket;
  }
  return DisplayFrame{};
}

std::vector<DisplayFrame> MeasurementPipeline::framesForDuration(double seconds) const {
  if (seconds <= 0.0) {
    return {};
  }
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  const qint64 oldestMs = now - static_cast<qint64>(seconds * 1000.0);

  std::lock_guard<std::mutex> lock(m_frameMutex);
  std::vector<DisplayFrame> out;
  // Pre-allocate rough estimate.
  out.reserve(m_frames.size());
  for (auto it = m_frames.rbegin(); it != m_frames.rend(); ++it) {
    if (it->timestampMs < oldestMs) {
      break;
    }
    out.push_back(*it);
  }
  std::reverse(out.begin(), out.end());
  return out;
}

std::vector<DisplayFrame> MeasurementPipeline::lastNFrames(std::size_t n) const {
  std::lock_guard<std::mutex> lock(m_frameMutex);
  std::vector<DisplayFrame> out;
  if (n == 0 || m_frames.empty()) {
    return out;
  }
  const std::size_t count = std::min(n, m_frames.size());
  out.reserve(count);
  auto it = m_frames.end() - static_cast<std::deque<DisplayFrame>::difference_type>(count);
  for (; it != m_frames.end(); ++it) {
    out.push_back(*it);
  }
  return out;
}

bool MeasurementPipeline::maxCurrentRawLastN(std::size_t n, double &maxCurrent) const {
  std::lock_guard<std::mutex> lock(m_rawMutex);
  if (m_rawWindow.empty() || n == 0) {
    return false;
  }
  const std::size_t count = std::min(n, m_rawWindow.size());
  maxCurrent = -std::numeric_limits<double>::infinity();
  for (std::size_t i = m_rawWindow.size() - count; i < m_rawWindow.size(); ++i) {
    if (m_rawWindow[i].current > maxCurrent) {
      maxCurrent = m_rawWindow[i].current;
    }
  }
  return true;
}

bool MeasurementPipeline::maxValuesRawLastN(std::size_t n, double &maxVoltage,
                                            double &maxCurrent,
                                            double &maxPower) const {
  std::lock_guard<std::mutex> lock(m_rawMutex);
  if (m_rawWindow.empty() || n == 0) {
    return false;
  }
  const std::size_t count = std::min(n, m_rawWindow.size());
  maxVoltage = maxCurrent = maxPower = -std::numeric_limits<double>::infinity();
  for (std::size_t i = m_rawWindow.size() - count; i < m_rawWindow.size(); ++i) {
    const PowerData &d = m_rawWindow[i];
    if (d.voltage > maxVoltage) maxVoltage = d.voltage;
    if (d.current > maxCurrent) maxCurrent = d.current;
    if (d.power > maxPower) maxPower = d.power;
  }
  return true;
}

double MeasurementPipeline::stdDevCurrentLastN(std::size_t n) const {
  auto frames = lastNFrames(n);
  if (frames.size() < 2) {
    return 0.0;
  }
  double sum = 0.0;
  for (const auto &f : frames) {
    sum += f.current;
  }
  const double mean = sum / static_cast<double>(frames.size());
  double sqSum = 0.0;
  for (const auto &f : frames) {
    const double diff = f.current - mean;
    sqSum += diff * diff;
  }
  return std::sqrt(sqSum / static_cast<double>(frames.size()));
}

void MeasurementPipeline::setFrameRate(int framesPerSecond) {
  int fps = std::clamp(framesPerSecond, 1, 120);
  m_frameRate.store(fps);
  const int intervalMs = static_cast<int>(std::round(1000.0 / fps));
  if (m_frameTimer) {
    m_frameTimer->setInterval(intervalMs);
  }
}

void MeasurementPipeline::setMaxHistoryDuration(double seconds) {
  m_maxHistorySeconds.store(std::max(seconds, 1.0));
  pruneFrames();
}

void MeasurementPipeline::onFrameTimer() {
  DisplayFrame frame;
  {
    std::lock_guard<std::mutex> lock(m_frameMutex);
    flushBucket();
    if (!m_frames.empty()) {
      frame = m_frames.back();
    }
  }
  if (frame.sampleCount > 0) {
    emit frameReady(frame);
  }
}

void MeasurementPipeline::flushBucket() {
  if (m_bucketHasData.exchange(false)) {
    m_frames.push_back(m_currentBucket);
    m_currentBucket = DisplayFrame{};
    pruneFrames();
  }
}

void MeasurementPipeline::pruneFrames() {
  if (m_frames.empty()) return;

  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  const qint64 oldestAllowedMs =
      now - static_cast<qint64>(m_maxHistorySeconds.load() * 1000.0);

  while (!m_frames.empty() && m_frames.front().timestampMs < oldestAllowedMs) {
    m_frames.pop_front();
  }
}
