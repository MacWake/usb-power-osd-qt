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
}

qint64 MeasurementPipeline::frameIntervalMs() const {
  return static_cast<qint64>(std::round(1000.0 / m_frameRate.load()));
}

qint64 MeasurementPipeline::bucketTimestampFor(qint64 sampleTimestampMs) const {
  const qint64 interval = frameIntervalMs();
  if (interval <= 0) {
    return sampleTimestampMs;
  }
  return (sampleTimestampMs / interval) * interval;
}

void MeasurementPipeline::pushSample(const PowerData &sample) {
  {
    std::lock_guard<std::mutex> lock(m_rawMutex);
    m_rawWindow.push_back(sample);
    while (m_rawWindow.size() > MaxRawWindowSize) {
      m_rawWindow.pop_front();
    }
  }

  std::lock_guard<std::mutex> lock(m_frameMutex);

  const qint64 wallNow = QDateTime::currentMSecsSinceEpoch();
  const qint64 sampleTs = static_cast<qint64>(sample.timestamp);
  const bool isValid = sample.current >=
                         static_cast<double>(m_minCurrentThreshold.load()) &&
                     sample.voltage >= 3.0;

  if (isValid) {
    m_lastValidCurrentMs = std::max(m_lastValidCurrentMs, sampleTs - m_pauseOffsetMs);
    m_lastValidWallClockMs = wallNow;
  }

  const qint64 threshold = m_pausedThresholdMs.load();
  const bool shouldPause =
      threshold > 0 && (wallNow - m_lastValidWallClockMs) > threshold;

  if (shouldPause) {
    m_isPaused = true;
  } else if (isValid) {
    m_isPaused = false;
  }

  // Map device-relative timestamps to local wall-clock time using an epoch
  // learned from the first valid sample. This preserves relative timing and
  // allows the display to reflect packet delays, while keeping serial and BLE
  // samples on the same time base.
  qint64 effectiveSampleTs = sampleTs;
  if (isValid) {
    if (!m_timestampEpochSet) {
      m_timestampEpochOffsetMs = wallNow - sampleTs;
      m_timestampEpochSet = true;
    }
    effectiveSampleTs = sampleTs + m_timestampEpochOffsetMs;
  } else if (!m_timestampEpochSet) {
    // No epoch yet; fall back to wall-clock so invalid samples don't land far
    // in the past/future before the first real measurement.
    effectiveSampleTs = wallNow;
  }

  // Compensate the stream time for the wall-clock pause duration so the graph
  // does not show a gap when data resumes after a pause/disconnect.
  if (m_isPaused) {
    if (m_pauseStartMs == 0) {
      m_pauseStartMs = wallNow;
    }
    return;
  }

  // We just resumed: add the elapsed pause time to the offset so the new
  // sample appears immediately after the last pre-pause bucket.
  if (m_pauseStartMs != 0) {
    m_pauseOffsetMs += wallNow - m_pauseStartMs;
    m_pauseStartMs = 0;
  }

  const qint64 streamTs = effectiveSampleTs - m_pauseOffsetMs;
  const qint64 bucketTs = bucketTimestampFor(streamTs);
  auto it = m_frameBuckets.find(bucketTs);
  if (it == m_frameBuckets.end()) {
    DisplayFrame frame;
    frame.timestampMs = bucketTs;
    frame.voltage = sample.voltage;
    frame.current = isValid ? sample.current : 0.0;
    frame.power = isValid ? sample.power : 0.0;
    frame.energyWh = sample.energy;
    frame.sampleCount = isValid ? 1 : 0;
    m_frameBuckets[bucketTs] = frame;
  } else {
    if (isValid) {
      it->second.current = std::max(it->second.current, sample.current);
      it->second.power = std::max(it->second.power, sample.power);
      ++it->second.sampleCount;
    }
    it->second.voltage = sample.voltage;
    it->second.energyWh = sample.energy;
  }

  // If a BLE packet arrived late/out of order, also flush any intermediate
  // buckets that are now older than this sample so the UI can see them.
  flushBucketsUpTo(bucketTs);
}

void MeasurementPipeline::reset() {
  {
    std::lock_guard<std::mutex> lock(m_rawMutex);
    m_rawWindow.clear();
  }
  {
    std::lock_guard<std::mutex> lock(m_frameMutex);
    m_frameBuckets.clear();
    m_lastValidCurrentMs = 0;
    m_lastValidWallClockMs = 0;
    m_pauseOffsetMs = 0;
    m_pauseStartMs = 0;
    m_timestampEpochOffsetMs = 0;
    m_timestampEpochSet = false;
    m_isPaused = false;
  }
}

DisplayFrame MeasurementPipeline::latestFrame() const {
  std::lock_guard<std::mutex> lock(m_frameMutex);
  if (m_frameBuckets.empty()) {
    return DisplayFrame{};
  }
  // Return the newest bucket, even if it has not been "closed" yet.
  return m_frameBuckets.rbegin()->second;
}

std::vector<DisplayFrame> MeasurementPipeline::buildFrameList() const {
  if (m_frameBuckets.empty()) {
    return {};
  }

  std::vector<DisplayFrame> out;
  out.reserve(m_frameBuckets.size());

  // Forward-fill gaps so slow sources produce a steady 30 fps stream.
  DisplayFrame lastValid;
  bool haveLastValid = false;
  for (const auto &[ts, frame] : m_frameBuckets) {
    if (frame.sampleCount > 0) {
      out.push_back(frame);
      lastValid = frame;
      haveLastValid = true;
    } else if (haveLastValid) {
      DisplayFrame filled = lastValid;
      filled.timestampMs = ts;
      out.push_back(filled);
    }
  }
  return out;
}

std::vector<DisplayFrame> MeasurementPipeline::framesForDuration(double seconds) const {
  if (seconds <= 0.0) {
    return {};
  }
  std::lock_guard<std::mutex> lock(m_frameMutex);
  auto frames = buildFrameList();
  if (frames.empty()) {
    return frames;
  }
  const qint64 newest = frames.back().timestampMs;
  const qint64 oldest = newest - static_cast<qint64>(seconds * 1000.0);
  std::vector<DisplayFrame> result;
  for (auto it = frames.rbegin(); it != frames.rend(); ++it) {
    if (it->timestampMs < oldest) {
      break;
    }
    result.push_back(*it);
  }
  std::reverse(result.begin(), result.end());
  return result;
}

std::vector<DisplayFrame> MeasurementPipeline::framesInRange(qint64 startMs,
                                                              qint64 endMs) const {
  if (startMs >= endMs) {
    return {};
  }
  std::lock_guard<std::mutex> lock(m_frameMutex);
  auto frames = buildFrameList();
  if (frames.empty()) {
    return frames;
  }
  std::vector<DisplayFrame> result;
  for (auto it = frames.rbegin(); it != frames.rend(); ++it) {
    if (it->timestampMs < startMs) {
      break;
    }
    if (it->timestampMs <= endMs) {
      result.push_back(*it);
    }
  }
  std::reverse(result.begin(), result.end());
  return result;
}

std::vector<DisplayFrame> MeasurementPipeline::lastNFrames(std::size_t n) const {
  std::lock_guard<std::mutex> lock(m_frameMutex);
  auto frames = buildFrameList();
  if (n == 0 || frames.empty()) {
    return {};
  }
  const std::size_t count = std::min(n, frames.size());
  return std::vector<DisplayFrame>(frames.end() - static_cast<std::vector<DisplayFrame>::difference_type>(count),
                                    frames.end());
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
  pruneBuckets();
}

void MeasurementPipeline::setMinCurrentThreshold(float minCurrent) {
  m_minCurrentThreshold.store(minCurrent);
}

void MeasurementPipeline::setPausedThresholdMs(qint64 ms) {
  m_pausedThresholdMs.store(std::max(qint64{0}, ms));
}

void MeasurementPipeline::onFrameTimer() {
  DisplayFrame frame;
  bool haveFrame = false;
  bool wasPaused;
  {
    std::lock_guard<std::mutex> lock(m_frameMutex);

    wasPaused = m_isPaused;

    // Determine newest data timestamp in stream time.
    qint64 newestDataTs = 0;
    for (auto it = m_frameBuckets.rbegin(); it != m_frameBuckets.rend(); ++it) {
      if (it->second.sampleCount > 0) {
        newestDataTs = it->first;
        break;
      }
    }

    if (newestDataTs > 0) {
      flushBucketsUpTo(newestDataTs);
      frame = m_frameBuckets.rbegin()->second;
      haveFrame = true;
    }

    pruneBuckets();
  }

  if (wasPaused != m_isPaused) {
    emit pausedChanged(m_isPaused);
  }

  if (haveFrame && frame.sampleCount > 0) {
    emit frameReady(frame);
  }
}

void MeasurementPipeline::flushBucketsUpTo(qint64 bucketTimestampMs) {
  if (m_frameBuckets.empty()) {
    return;
  }

  // Ensure there is a bucket entry for every timestamp between the oldest
  // bucket and the given one. Empty entries will be forward-filled later.
  const qint64 oldest = m_frameBuckets.begin()->first;
  const qint64 interval = frameIntervalMs();
  if (interval <= 0) {
    return;
  }
  for (qint64 ts = oldest + interval; ts <= bucketTimestampMs; ts += interval) {
    if (!m_frameBuckets.count(ts)) {
      m_frameBuckets[ts] = DisplayFrame{};
      m_frameBuckets[ts].timestampMs = ts;
    }
  }
}

void MeasurementPipeline::pruneBuckets() {
  if (m_frameBuckets.empty()) {
    return;
  }
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  const qint64 oldestAllowedTs =
      bucketTimestampFor(now - static_cast<qint64>(m_maxHistorySeconds.load() * 1000.0));
  auto it = m_frameBuckets.begin();
  while (it != m_frameBuckets.end() && it->first < oldestAllowedTs) {
    it = m_frameBuckets.erase(it);
  }
}
