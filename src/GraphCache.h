#pragma once

#include "MeasurementPipeline.h"

#include <QtGlobal>
#include <vector>

/**
 * @brief Per-pixel graph data cache.
 *
 * Holds one Entry per horizontal pixel column, ordered newest-first.
 * - entries[0] is the rightmost (newest) pixel.
 * - entries[width-1] is the leftmost (oldest) pixel.
 *
 * An entry is considered invalid (null / no-draw) when its voltage is negative.
 * This keeps the entry struct small (16 bytes) and avoids a bool that would be
 * padded to 64-bit alignment.
 *
 * Supports both linear and logarithmic (stepped) time-to-pixel mappings.
 * In linear live mode only the newest pixel columns are updated incrementally;
 * in history/review mode and on mapping changes the whole cache is rebuilt.
 */
class GraphCache {
public:
  struct Entry {
    double current = 0.0;
    double voltage = -1.0; // < 0 means invalid / no draw
  };

  GraphCache() = default;

  void setSize(int width);
  [[nodiscard]] int size() const { return m_width; }

  void setParams(bool logScale, double pixelsPerSecond, bool isLive);

  /**
   * @brief Rebuild the entire cache from a frame list.
   *
   * Used in history/review mode and when the mapping changes. History mode is
   * always linear regardless of the logScale setting.
   */
  void rebuild(const std::vector<DisplayFrame> &frames, qint64 viewRightMs);

  /**
   * @brief Incrementally update the cache for live mode.
   */
  void updateLive(const std::vector<DisplayFrame> &frames,
                  qint64 viewRightMs);

  [[nodiscard]] const std::vector<Entry> &entries() const { return m_entries; }

  [[nodiscard]] bool logScale() const { return m_logScale; }
  [[nodiscard]] bool hasData() const { return m_hasData; }
  [[nodiscard]] double minCurrent() const { return m_minCurrent; }
  [[nodiscard]] double maxCurrent() const { return m_maxCurrent; }

private:
  [[nodiscard]] double unitMs() const;
  [[nodiscard]] int pixelIndexLinear(qint64 timestampMs,
                                      qint64 viewRightMs) const;

  static double steppedAgeForPixel(int idx, int width, double pixelsPerSecond);

  void fillLinear(const std::vector<DisplayFrame> &frames,
                  qint64 viewRightMs, int startIdx, int endIdx, bool clear,
                  bool preserveNewest);
  void fillLog(const std::vector<DisplayFrame> &frames, qint64 viewRightMs);
  void updateStats();

  void invalidateEntry(Entry &entry);

  int m_width = 0;
  bool m_logScale = false;
  double m_pixelsPerSecond = 30.0;
  bool m_isLive = true;

  std::vector<Entry> m_entries;
  qint64 m_viewRightMs = 0;

  bool m_hasData = false;
  double m_minCurrent = 0.0;
  double m_maxCurrent = 0.0;
};
