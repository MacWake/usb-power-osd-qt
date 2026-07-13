#include "GraphCache.h"

#include "MeasurementPipeline.h"

#include <algorithm>
#include <cmath>
#include <limits>

void GraphCache::setSize(int width) {
  if (width == m_width) {
    return;
  }

  std::vector<Entry> newEntries(width, Entry{});
  const int copyCount = std::min(width, m_width);
  for (int i = 0; i < copyCount; ++i) {
    newEntries[i] = m_entries[i];
  }

  m_entries = std::move(newEntries);
  m_width = width;
}

void GraphCache::setParams(bool logScale, double pixelsPerSecond, bool isLive) {
  const bool changed =
      m_logScale != logScale ||
      std::abs(m_pixelsPerSecond - pixelsPerSecond) > 0.0001 ||
      m_isLive != isLive;

  m_logScale = logScale;
  m_pixelsPerSecond = pixelsPerSecond;
  m_isLive = isLive;

  if (changed) {
    // Invalidate cache so the next call rebuilds.
    m_viewRightMs = 0;
  }
}

double GraphCache::unitMs() const {
  return static_cast<double>(
      MeasurementPipeline::pixelsPerSecondToIntervalMs(m_pixelsPerSecond));
}

void GraphCache::invalidateEntry(Entry &entry) { entry.voltage = -1.0; }

int GraphCache::pixelIndexLinear(qint64 timestampMs, qint64 viewRightMs) const {
  const double unit = unitMs();
  const double age = static_cast<double>(viewRightMs - timestampMs);
  const int idx = static_cast<int>(std::floor(age / unit));
  if (idx < 0 || idx >= m_width) {
    return -1;
  }
  return idx;
}

// Maps a horizontal pixel index (0 = right/newest, w-1 = left/oldest) to a
// time offset in seconds from the right edge for the stepped "log-like" view.
// The newest 2/6 (1/3) uses linear resolution. The remaining 4/6 are divided
// into four segments, each with half the resolution of the one before it:
// 1x, 2x, 4x, 8x, 16x. Leftover pixels from integer division are added to the
// leftmost segment.
double GraphCache::steppedAgeForPixel(int idx, int width,
                                       double pixelsPerSecond) {
  const qint64 intervalMs =
      MeasurementPipeline::pixelsPerSecondToIntervalMs(pixelsPerSecond);
  const double unit = static_cast<double>(intervalMs) / 1000.0;
  const int baseSegSize = width / 6;
  const int leftover = width - 6 * baseSegSize;

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

void GraphCache::fillLinear(const std::vector<DisplayFrame> &frames,
                            qint64 viewRightMs, int startIdx, int endIdx,
                            bool clear, bool preserveNewest) {
  const int fillEnd = std::min(endIdx, m_width);

  if (clear) {
    for (int i = startIdx; i < fillEnd; ++i) {
      invalidateEntry(m_entries[i]);
    }
  }

  for (const auto &frame : frames) {
    const int idx = pixelIndexLinear(frame.timestampMs, viewRightMs);
    if (idx < startIdx || idx >= fillEnd) {
      continue;
    }

    // In live mode we only want to overwrite the rightmost pixel when the
    // view has actually advanced by a whole pixel. Otherwise the newest
    // sample keeps overwriting the same pixel and makes the line jitter.
    if (preserveNewest && idx == 0) {
      continue;
    }

    Entry &entry = m_entries[idx];
    if (entry.voltage < 0.0 || frame.current > entry.current) {
      entry.current = frame.current;
      entry.voltage = frame.voltage;
    }
  }
}

void GraphCache::fillLog(const std::vector<DisplayFrame> &frames,
                         qint64 viewRightMs) {
  const int w = m_width;
  if (w == 0) {
    return;
  }
  const double pps = std::max(m_pixelsPerSecond, 0.1);

  // Invalidate every pixel first; a direct age-bin can leave entries null
  // anywhere there is no source data.
  for (int i = 0; i < w; ++i) {
    invalidateEntry(m_entries[i]);
  }

  // Bin frames directly by the age range each pixel represents. This avoids
  // the hierarchical 2:1 derivation that previously placed newly-arrived data
  // at far-left "older" pixels, which looked like historic pre-filling.
  for (int i = 0; i < w; ++i) {
    const double leftSeconds = steppedAgeForPixel(i, w, pps);
    const double rightSeconds = steppedAgeForPixel(i + 1, w, pps);

    const qint64 tRight =
        viewRightMs - static_cast<qint64>(leftSeconds * 1000.0);
    const qint64 tLeft =
        viewRightMs - static_cast<qint64>(rightSeconds * 1000.0);
    if (tLeft >= tRight) {
      continue;
    }

    bool found = false;
    double maxCurrent = 0.0;
    double voltage = 0.0;
    for (const auto &frame : frames) {
      if (frame.timestampMs <= tRight && frame.timestampMs > tLeft) {
        if (!found || frame.current > maxCurrent) {
          maxCurrent = frame.current;
          voltage = frame.voltage;
          found = true;
        }
      }
    }

    if (found) {
      m_entries[i].current = maxCurrent;
      m_entries[i].voltage = voltage;
    }
  }
}

void GraphCache::rebuild(const std::vector<DisplayFrame> &frames,
                         qint64 viewRightMs) {
  if (m_width == 0) {
    return;
  }

  m_viewRightMs = viewRightMs;

  if (m_logScale) {
    fillLog(frames, viewRightMs);
  } else {
    fillLinear(frames, viewRightMs, 0, m_width, true, false);
  }

  updateStats();
}

void GraphCache::updateLive(const std::vector<DisplayFrame> &frames,
                            qint64 viewRightMs) {
  if (m_width == 0) {
    return;
  }

  if (m_viewRightMs == 0 ||
      static_cast<int>(m_entries.size()) != m_width) {
    rebuild(frames, viewRightMs);
    return;
  }

  // Log mode needs a full re-bin for every view shift because a sub-pixel
  // change in the right edge can move frames between segments.
  if (m_logScale) {
    rebuild(frames, viewRightMs);
    return;
  }

  const double unit = unitMs();
  const int scrollPixels =
      static_cast<int>(std::floor((viewRightMs - m_viewRightMs) / unit));

  if (std::abs(scrollPixels) >= m_width) {
    rebuild(frames, viewRightMs);
    return;
  }

  if (scrollPixels > 0) {
    // Shift existing entries towards older (higher) indices, newest-first.
    for (int i = m_width - 1; i >= scrollPixels; --i) {
      m_entries[i] = m_entries[i - scrollPixels];
    }
    // Only the entries freed by the shift become null.
    for (int i = 0; i < scrollPixels; ++i) {
      invalidateEntry(m_entries[i]);
    }
  }

  // Refill the newest columns. Always include at least the rightmost pixel;
  // include the columns freed by scrolling.
  const int fillEnd = std::max(scrollPixels, 1) + 1;

  // Preserve the newest pixel during sub-pixel view advances so a noisy latest
  // sample does not jitter over the previously drawn rightmost value.
  const bool preserveNewest = scrollPixels == 0;
  fillLinear(frames, viewRightMs, 0, fillEnd, false, preserveNewest);

  m_viewRightMs = viewRightMs;

  updateStats();
}

void GraphCache::updateStats() {
  bool hasData = false;
  double minCurrent = std::numeric_limits<double>::max();
  double maxCurrent = std::numeric_limits<double>::lowest();

  for (const auto &entry : m_entries) {
    if (entry.voltage >= 0.0) {
      hasData = true;
      minCurrent = std::min(minCurrent, entry.current);
      maxCurrent = std::max(maxCurrent, entry.current);
    }
  }

  m_hasData = hasData;
  m_minCurrent = hasData ? minCurrent : 0.0;
  m_maxCurrent = hasData ? maxCurrent : 0.0;
}
