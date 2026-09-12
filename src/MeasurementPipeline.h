// MeasurementPipeline.h
#pragma once

#include "PowerData.h"
#include "PowerDataSource.h"

#include <QObject>
#include <QTimer>
#include <atomic>
#include <cstddef>
#include <deque>
#include <map>
#include <mutex>
#include <vector>

/**
 * @brief A single time-normalized display sample produced at a fixed frame rate.
 *
 * Aggregates all raw PowerData samples that arrived during one display bucket.
 * Current/power keep the maximum value seen in the bucket so short spikes
 * remain visible; voltage and energy use the latest value.
 */
struct DisplayFrame {
  qint64 timestampMs = 0; // end of bucket
  double voltage = 0.0;
  double current = 0.0;
  double power = 0.0;
  double energyWh = 0.0;
  std::size_t sampleCount = 0;
};

/**
 * @brief Decouples high-rate backend acquisition from the UI render loop.
 *
 * - Receives PowerData at full device rate (pushSample).
 * - Buckets samples by their own timestamp on a fixed 30 Hz grid.
 * - Within a bucket, current/power are aggregated with max() to keep spikes.
 * - For slower sources, empty buckets are filled by repeating the last sample.
 * - History collection pauses when current stays below threshold for 1 s.
 * - Provides statistics for labels/audio over configurable windows.
 */
class MeasurementPipeline : public QObject {
  Q_OBJECT

public:
  explicit MeasurementPipeline(QObject *parent = nullptr);

  void pushSample(const PowerData &sample);
  void reset();

  [[nodiscard]] DisplayFrame latestFrame() const;
  [[nodiscard]] std::vector<DisplayFrame> framesForDuration(double seconds) const;
  [[nodiscard]] std::vector<DisplayFrame> framesInRange(qint64 startMs,
                                                         qint64 endMs) const;
  [[nodiscard]] std::vector<DisplayFrame> lastNFrames(std::size_t n) const;

  [[nodiscard]] bool maxCurrentRawLastN(std::size_t n, double &maxCurrent) const;
  [[nodiscard]] bool maxValuesRawLastN(std::size_t n, double &maxVoltage,
                                       double &maxCurrent,
                                       double &maxPower) const;
  [[nodiscard]] double stdDevCurrentLastN(std::size_t n) const;

  void setFrameRate(int framesPerSecond);
  [[nodiscard]] int frameRate() const { return m_frameRate.load(); }

  void setMaxHistoryDuration(double seconds);
  [[nodiscard]] double maxHistoryDuration() const { return m_maxHistorySeconds.load(); }

  void setMinCurrentThreshold(float minCurrent);
  void setPausedThresholdMs(qint64 ms);

  static constexpr int DefaultFrameRate = 30;
  static constexpr double DefaultPixelsPerSecond = 4.0;
  // Enough history for a 4K display at 1 px/s plus review navigation.
  static constexpr double DefaultHistorySeconds = 3600.0;

  // Frame timestamps are quantized to this fixed grid (milliseconds per frame).
  [[nodiscard]] qint64 frameIntervalMs() const;

  /**
   * @brief Pixel width in milliseconds that matches the frame grid.
   *
   * The graph uses pixels-per-second from settings, but the actual scroll grid
   * must align with the pipeline's integer frame interval. Otherwise the live
   * right edge drifts relative to the bucket timestamps and the graph appears
   * to freeze and jump.
   */
  [[nodiscard]] static qint64 pixelsPerSecondToIntervalMs(double pixelsPerSecond);
  [[nodiscard]] bool isPaused() const { return m_isPaused; }

signals:
  void frameReady(const DisplayFrame &frame);
  void pausedChanged(bool paused);

private slots:
  void onFrameTimer();

private:
  mutable std::mutex m_rawMutex;
  std::deque<PowerData> m_rawWindow; // small rolling window for label stats
  static constexpr std::size_t MaxRawWindowSize = 10;

  mutable std::mutex m_frameMutex;
  // Buckets keyed by quantized timestamp. A map handles out-of-order BLE packets.
  std::map<qint64, DisplayFrame> m_frameBuckets;
  std::atomic<double> m_maxHistorySeconds{DefaultHistorySeconds};

  std::atomic<float> m_minCurrentThreshold{0.0f};
  std::atomic<qint64> m_pausedThresholdMs{1000};
  qint64 m_lastValidCurrentMs = 0;   // last sample timestamp with current >= threshold
  qint64 m_lastValidWallClockMs = 0; // for real-time pause detection
  qint64 m_pauseOffsetMs = 0;        // subtract from sample timestamps to keep stream contiguous
  qint64 m_pauseStartMs = 0;           // wall-clock time when pause began
  qint64 m_timestampEpochOffsetMs = 0; // offset to map device-relative timestamps to local wall-clock
  bool m_timestampEpochSet = false;    // true once the first valid sample has set the epoch
  bool m_isPaused = false;

  QTimer *m_frameTimer = nullptr;
  std::atomic<int> m_frameRate{DefaultFrameRate};

  void flushBucketsUpTo(qint64 bucketTimestampMs);
  [[nodiscard]] qint64 bucketTimestampFor(qint64 sampleTimestampMs) const;
  void pruneBuckets();
  [[nodiscard]] std::vector<DisplayFrame> buildFrameList() const;
};
