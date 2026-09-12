#include "test_graph_cache.h"

#include "GraphCache.h"
#include "MeasurementPipeline.h"

#include <QTest>

namespace {

DisplayFrame makeFrame(qint64 timestampMs, double current, double voltage) {
  DisplayFrame f;
  f.timestampMs = timestampMs;
  f.current = current;
  f.voltage = voltage;
  f.power = current * voltage;
  f.energyWh = 0.0;
  f.sampleCount = 1;
  return f;
}

bool entryIsValid(const GraphCache::Entry &e) { return e.voltage >= 0.0; }

} // namespace

void TestGraphCache::oneEntryPerPixel() {
  GraphCache cache;
  cache.setSize(100);
  QCOMPARE(cache.size(), 100);
  QCOMPARE(static_cast<int>(cache.entries().size()), 100);
}

void TestGraphCache::newestFirstOrder() {
  GraphCache cache;
  cache.setSize(100);
  cache.setParams(false, 30.0, true);

  const qint64 viewRight = 1000;
  const double unit = 1000.0 / 30.0;
  // Place frame 10 ms before the right edge: it belongs to the newest pixel.
  auto frames = std::vector{makeFrame(viewRight - 10, 1.5, 5.0)};
  cache.rebuild(frames, viewRight);

  QVERIFY(entryIsValid(cache.entries()[0]));
  QCOMPARE(cache.entries()[0].current, 1.5);
  QCOMPARE(cache.entries()[0].voltage, 5.0);

  // Entries one pixel to the left should be invalid with no data.
  QVERIFY(!entryIsValid(cache.entries()[1]));
}

void TestGraphCache::resizePreservesNewestData() {
  GraphCache cache;
  cache.setSize(100);
  cache.setParams(false, 30.0, true);

  const qint64 viewRight = 1000;
  auto frames = std::vector{makeFrame(viewRight - 10, 2.5, 5.0)};
  cache.rebuild(frames, viewRight);

  cache.setSize(120);
  QCOMPARE(cache.size(), 120);
  QVERIFY(entryIsValid(cache.entries()[0]));
  QCOMPARE(cache.entries()[0].current, 2.5);
}

void TestGraphCache::resizeAddsNullSlotsOnGrowth() {
  GraphCache cache;
  cache.setSize(100);
  cache.setParams(false, 30.0, true);

  const qint64 viewRight = 1000;
  auto frames = std::vector{makeFrame(viewRight - 10, 2.5, 5.0)};
  cache.rebuild(frames, viewRight);

  cache.setSize(120);
  // Newly added slots on the left (older side) must be null/invalid.
  QVERIFY(!entryIsValid(cache.entries()[119]));
}

void TestGraphCache::historyRebuildIsLinear() {
  GraphCache cache;
  cache.setSize(60);
  cache.setParams(false, 30.0, false); // history mode

  const qint64 viewRight = 2000;
  const double unit = 1000.0 / 30.0;
  std::vector<DisplayFrame> frames;
  for (int i = 0; i < 30; ++i) {
    frames.push_back(makeFrame(viewRight - static_cast<qint64>(i * unit) - 10,
                                1.0, 5.0));
  }

  cache.rebuild(frames, viewRight);

  // Each of the 30 newest pixels should have one frame.
  for (int i = 0; i < 30; ++i) {
    QVERIFY(entryIsValid(cache.entries()[i]));
    QCOMPARE(cache.entries()[i].current, 1.0);
  }
  // Older pixels are null.
  for (int i = 30; i < 60; ++i) {
    QVERIFY(!entryIsValid(cache.entries()[i]));
  }
}

void TestGraphCache::historyRebuildClearsOldData() {
  GraphCache cache;
  cache.setSize(100);
  cache.setParams(false, 30.0, true);

  cache.rebuild({makeFrame(1000, 1.0, 5.0)}, 1000);
  QVERIFY(entryIsValid(cache.entries()[0]));

  // Rebuild with no frames in a different view window.
  cache.rebuild({}, 2000);
  QVERIFY(!cache.hasData());
}

void TestGraphCache::liveLinearAppendsNewestAtIndexZero() {
  GraphCache cache;
  cache.setSize(100);
  cache.setParams(false, 30.0, true);

  const double unit = 1000.0 / 30.0;
  const qint64 right1 = 1000;
  cache.updateLive({makeFrame(right1 - 10, 1.0, 5.0)}, right1);

  QVERIFY(entryIsValid(cache.entries()[0]));
  QCOMPARE(cache.entries()[0].current, 1.0);

  const qint64 right2 = right1 + static_cast<qint64>(std::ceil(unit));
  cache.updateLive({makeFrame(right2 - 10, 2.0, 5.0)}, right2);

  QVERIFY(entryIsValid(cache.entries()[0]));
  QCOMPARE(cache.entries()[0].current, 2.0);
}

void TestGraphCache::liveLinearShiftsOlderEntriesRight() {
  GraphCache cache;
  cache.setSize(100);
  cache.setParams(false, 30.0, true);

  const double unit = 1000.0 / 30.0;
  const qint64 right1 = 1000;
  cache.updateLive({makeFrame(right1 - 10, 1.0, 5.0)}, right1);

  const qint64 right2 = right1 + static_cast<qint64>(std::ceil(unit));
  cache.updateLive({makeFrame(right2 - 10, 2.0, 5.0)}, right2);

  // The old rightmost entry should now be one pixel to the left.
  QVERIFY(entryIsValid(cache.entries()[1]));
  QCOMPARE(cache.entries()[1].current, 1.0);
}

void TestGraphCache::liveLinearScrollsByWholePixels() {
  GraphCache cache;
  cache.setSize(100);
  cache.setParams(false, 30.0, true);

  const double unit = 1000.0 / 30.0;
  const qint64 right1 = static_cast<qint64>(10 * unit);
  cache.updateLive({makeFrame(right1 - 10, 1.0, 5.0)}, right1);

  // Advance by exactly one pixel's worth of milliseconds.
  const qint64 right2 = right1 + static_cast<qint64>(std::ceil(unit));
  cache.updateLive({makeFrame(right2 - 10, 2.0, 5.0)}, right2);

  QVERIFY(entryIsValid(cache.entries()[0]));
  QCOMPARE(cache.entries()[0].current, 2.0);
  QVERIFY(entryIsValid(cache.entries()[1]));
  QCOMPARE(cache.entries()[1].current, 1.0);

  // No other entries should be valid.
  int validCount = 0;
  for (const auto &e : cache.entries()) {
    if (entryIsValid(e)) ++validCount;
  }
  QCOMPARE(validCount, 2);
}

void TestGraphCache::liveLinearLeavesNullSlots() {
  GraphCache cache;
  cache.setSize(100);
  cache.setParams(false, 30.0, true);

  const qint64 right = 1000;
  cache.updateLive({makeFrame(right - 10, 1.0, 5.0)}, right);

  // Most of the cache should remain null/invalid.
  int validCount = 0;
  for (const auto &e : cache.entries()) {
    if (entryIsValid(e)) ++validCount;
  }
  QCOMPARE(validCount, 1);
}

void TestGraphCache::liveLinearKeepsMaxCurrentPerPixel() {
  GraphCache cache;
  cache.setSize(100);
  cache.setParams(false, 30.0, true);

  const double unit = 1000.0 / 30.0;
  const qint64 right = static_cast<qint64>(10 * unit);
  // Two frames in the newest pixel: lower and higher current.
  cache.updateLive(
      {
          makeFrame(right - 10, 0.5, 5.0),
          makeFrame(right - 20, 2.0, 5.0),
      },
      right);

  QVERIFY(entryIsValid(cache.entries()[0]));
  QCOMPARE(cache.entries()[0].current, 2.0);
}

void TestGraphCache::statsComputedFromEntries() {
  GraphCache cache;
  cache.setSize(100);
  cache.setParams(false, 30.0, true);

  const qint64 right = 1000;
  cache.rebuild(
      {
          makeFrame(right - 10, 0.5, 5.0),
          makeFrame(right - 50, 2.5, 5.0),
          makeFrame(right - 90, 1.5, 5.0),
      },
      right);

  QVERIFY(cache.hasData());
  QCOMPARE(cache.minCurrent(), 0.5);
  QCOMPARE(cache.maxCurrent(), 2.5);
}

void TestGraphCache::logRebuildFillsAllPixels() {
  GraphCache cache;
  cache.setSize(60); // width divisible by 6
  cache.setParams(true, 30.0, false);

  const qint64 right = 1000;
  // Enough frames to cover the whole stepped window.
  const double unit = 1000.0 / 30.0;
  std::vector<DisplayFrame> frames;
  for (int i = 0; i < 200; ++i) {
    frames.push_back(makeFrame(right - static_cast<qint64>(i * unit / 4),
                                1.0, 5.0));
  }

  cache.rebuild(frames, right);

  // The newest 20 pixels (segment 0) must be valid.
  for (int i = 0; i < 20; ++i) {
    QVERIFY(entryIsValid(cache.entries()[i]));
  }

  // Lower-resolution pixels should also be derived.
  int validLower = 0;
  for (int i = 20; i < 60; ++i) {
    if (entryIsValid(cache.entries()[i])) ++validLower;
  }
  QVERIFY(validLower > 0);
}

void TestGraphCache::logBinsFramesByAge() {
  GraphCache cache;
  cache.setSize(60);
  cache.setParams(true, 30.0, true);

  const qint64 right = 3000;
  const double unit = 1000.0 / 30.0;

  // Place a high-current spike at age ~1.0 s, which in this 60-pixel, 30 pps
  // stepped layout lands in segment 1 (the first lower-resolution segment).
  std::vector<DisplayFrame> frames;
  frames.push_back(makeFrame(right - static_cast<qint64>(unit / 2),
                              1.0, 5.0)); // newest pixel
  frames.push_back(makeFrame(right - 500, 1.0, 5.0)); // still segment 0
  frames.push_back(makeFrame(right - 1000, 7.0, 5.0)); // segment 1

  cache.rebuild(frames, right);

  // The high current must appear in the lower-resolution segment that
  // corresponds to its actual age, not in every left-side pixel.
  bool foundHighInLower = false;
  for (int i = 20; i < 60; ++i) {
    if (entryIsValid(cache.entries()[i]) && cache.entries()[i].current == 7.0) {
      foundHighInLower = true;
      break;
    }
  }
  QVERIFY(foundHighInLower);

  // Far-left pixels (older than any supplied frame) must stay invalid.
  bool hasInvalidFarLeft = false;
  for (int i = 30; i < 60; ++i) {
    if (!entryIsValid(cache.entries()[i])) {
      hasInvalidFarLeft = true;
      break;
    }
  }
  QVERIFY(hasInvalidFarLeft);
}

void TestGraphCache::logLiveUpdateIsStableForSubPixelAdvance() {
  GraphCache cache;
  cache.setSize(60);
  cache.setParams(true, 30.0, true);

  const double unit = 1000.0 / 30.0;
  const qint64 right1 = static_cast<qint64>(10 * unit);
  std::vector<DisplayFrame> frames;
  for (int i = 0; i < 300; ++i) {
    frames.push_back(makeFrame(
        right1 - static_cast<qint64>(i * unit / 4), 1.0, 5.0));
  }
  cache.updateLive(frames, right1);

  // Record lower-resolution entries.
  std::vector<double> lowerBefore;
  for (int i = 20; i < 60; ++i) {
    lowerBefore.push_back(entryIsValid(cache.entries()[i])
                              ? cache.entries()[i].current
                              : -1.0);
  }

  // Advance by less than one 1x pixel.
  const qint64 right2 = right1 + static_cast<qint64>(unit / 3);
  cache.updateLive(frames, right2);

  // Most lower-resolution entries should remain unchanged.
  int unchanged = 0;
  for (int i = 20; i < 60; ++i) {
    const double after = entryIsValid(cache.entries()[i])
                             ? cache.entries()[i].current
                             : -1.0;
    if (after == lowerBefore[i - 20]) ++unchanged;
  }
  QVERIFY(unchanged >= 35); // allow a few edge pixels to change
}

void TestGraphCache::logInvalidEntriesDoNotDraw() {
  GraphCache cache;
  cache.setSize(60);
  cache.setParams(true, 30.0, true);

  const qint64 right = 1000;
  // Only one frame.
  cache.rebuild({makeFrame(right - 10, 1.0, 5.0)}, right);

  // Newest high-res pixel valid.
  QVERIFY(entryIsValid(cache.entries()[0]));

  // Lower-resolution pixels derived from invalid sources should be invalid.
  // At least some far-left pixels must be null.
  bool hasInvalid = false;
  for (int i = 50; i < 60; ++i) {
    if (!entryIsValid(cache.entries()[i])) {
      hasInvalid = true;
      break;
    }
  }
  QVERIFY(hasInvalid);
}

void TestGraphCache::liveLinearPreservesNewestPixelUntilScroll() {
  GraphCache cache;
  cache.setSize(100);
  cache.setParams(false, 30.0, true);

  const double unit = 1000.0 / 30.0;
  const qint64 right = static_cast<qint64>(1000 * unit);
  cache.updateLive({makeFrame(right - 10, 1.0, 5.0)}, right);

  // The rightmost pixel is now 1.0 A.
  QCOMPARE(cache.entries()[0].current, 1.0);

  // A sub-pixel advance with a different current must NOT overwrite index 0,
  // because the view has not actually scrolled to a new pixel column yet.
  const qint64 right2 = right + static_cast<qint64>(unit / 3);
  cache.updateLive({makeFrame(right2 - 10, 9.0, 5.0)}, right2);
  QCOMPARE(cache.entries()[0].current, 1.0);

  // Once the view advances by more than a full pixel, the newest column is
  // updated and the previous value shifts one pixel to the left.
  const qint64 right3 = right + static_cast<qint64>(2 * unit);
  cache.updateLive({makeFrame(right3 - 10, 2.0, 5.0)}, right3);
  QCOMPARE(cache.entries()[0].current, 2.0);
  QCOMPARE(cache.entries()[1].current, 1.0);
}
