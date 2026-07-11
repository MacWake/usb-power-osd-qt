// MeasurementPipeline.h
#pragma once

#include "PowerData.h"
#include "PowerDataSource.h"

#include <QObject>
#include <QTimer>
#include <atomic>
#include <cstddef>
#include <deque>
#include <mutex>
#include <vector>

/**
 * @brief A single time-normalized display sample produced at a fixed frame rate.
 *
 * Aggregates all raw PowerData samples that arrived during one display bucket.
 * By default current/power keep the maximum value seen in the bucket so spikes
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
 * - Emits a DisplayFrame at a fixed rate (default 30 Hz).
 * - Keeps a rolling history of DisplayFrames for the time-based graph.
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

  static constexpr int DefaultFrameRate = 30;
  static constexpr double DefaultPixelsPerSecond = 4.0;
  // Enough history for a 4K display at 1 px/s plus review navigation.
  static constexpr double DefaultHistorySeconds = 3600.0;

signals:
  void frameReady(const DisplayFrame &frame);

private slots:
  void onFrameTimer();

private:
  mutable std::mutex m_rawMutex;
  std::deque<PowerData> m_rawWindow; // small rolling window for label stats
  static constexpr std::size_t MaxRawWindowSize = 10;

  mutable std::mutex m_frameMutex;
  std::deque<DisplayFrame> m_frames;
  std::atomic<double> m_maxHistorySeconds{DefaultHistorySeconds};

  DisplayFrame m_currentBucket{};
  std::atomic<bool> m_bucketHasData{false};

  QTimer *m_frameTimer = nullptr;
  std::atomic<int> m_frameRate{DefaultFrameRate};

  void flushBucket();
  void pruneFrames();
};
