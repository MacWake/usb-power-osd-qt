#include "test_graph_mapping.h"

#include "CurrentGraph.h"
#include "MeasurementPipeline.h"

#include <QTest>

namespace {

std::vector<DisplayFrame> makeConstantFrames(int count, qint64 startMs,
                                             qint64 stepMs) {
  std::vector<DisplayFrame> frames;
  frames.reserve(count);
  for (int i = 0; i < count; ++i) {
    DisplayFrame f;
    f.timestampMs = startMs + i * stepMs;
    f.voltage = 5.0;
    f.current = 1.0;
    f.power = 5.0;
    f.energyWh = 0.0;
    f.sampleCount = 1;
    frames.push_back(f);
  }
  return frames;
}

} // namespace

void TestGraphMapping::linearMappingIsStableUnderSubPixelTimeAdvance() {
  constexpr int width = 400;
  constexpr double pixelsPerSecond = 4.0;
  const double msPerPixel = 1000.0 / pixelsPerSecond; // 250 ms/pixel

  auto frames = makeConstantFrames(10, 0, 250);

  const qint64 newest1 = 2250;
  const qint64 oldest1 =
      newest1 - static_cast<qint64>(width / pixelsPerSecond * 1000.0);

  std::vector<double> current1;
  std::vector<double> voltage1;
  CurrentGraph::buildPixelMaps(width, false, pixelsPerSecond, newest1, oldest1,
                               frames, current1, voltage1);

  // Second paint with newest advanced by only 50 ms (well under one pixel).
  const qint64 newest2 = newest1 + 50;
  const qint64 oldest2 =
      newest2 - static_cast<qint64>(width / pixelsPerSecond * 1000.0);

  std::vector<double> current2;
  std::vector<double> voltage2;
  CurrentGraph::buildPixelMaps(width, false, pixelsPerSecond, newest2, oldest2,
                               frames, current2, voltage2);

  QCOMPARE(current1.size(), static_cast<std::size_t>(width));
  QCOMPARE(current2.size(), static_cast<std::size_t>(width));

  for (int i = 0; i < width; ++i) {
    QCOMPARE(current1[i], current2[i]);
    QCOMPARE(voltage1[i], voltage2[i]);
  }
}

void TestGraphMapping::linearMappingScrollsByWholePixel() {
  constexpr int width = 400;
  constexpr double pixelsPerSecond = 4.0;
  const double msPerPixel = 1000.0 / pixelsPerSecond;

  auto frames = makeConstantFrames(20, 0, 250);

  const qint64 newest1 = 4750;
  const qint64 oldest1 =
      newest1 - static_cast<qint64>(width / pixelsPerSecond * 1000.0);

  std::vector<double> current1;
  std::vector<double> voltage1;
  CurrentGraph::buildPixelMaps(width, false, pixelsPerSecond, newest1, oldest1,
                               frames, current1, voltage1);

  const qint64 newest2 = newest1 + static_cast<qint64>(msPerPixel);
  const qint64 oldest2 =
      newest2 - static_cast<qint64>(width / pixelsPerSecond * 1000.0);

  std::vector<double> current2;
  std::vector<double> voltage2;
  CurrentGraph::buildPixelMaps(width, false, pixelsPerSecond, newest2, oldest2,
                               frames, current2, voltage2);

  for (int i = 0; i < width - 1; ++i) {
    QCOMPARE(current1[i + 1], current2[i]);
    QCOMPARE(voltage1[i + 1], voltage2[i]);
  }
}

void TestGraphMapping::linearMappingKeepsMaxCurrentPerPixel() {
  constexpr int width = 100;
  constexpr double pixelsPerSecond = 4.0;
  const double msPerPixel = 1000.0 / pixelsPerSecond;

  const qint64 newest = 1000;
  const qint64 oldest = newest - static_cast<qint64>(width * msPerPixel);

  // Two frames in the same pixel column: one with 0.5 A, one with 2.0 A.
  std::vector<DisplayFrame> frames;
  DisplayFrame f1;
  f1.timestampMs = newest - static_cast<qint64>(msPerPixel / 4);
  f1.voltage = 5.0;
  f1.current = 0.5;
  f1.power = 2.5;
  frames.push_back(f1);

  DisplayFrame f2;
  f2.timestampMs = newest - static_cast<qint64>(3 * msPerPixel / 4);
  f2.voltage = 5.0;
  f2.current = 2.0;
  f2.power = 10.0;
  frames.push_back(f2);

  std::vector<double> current;
  std::vector<double> voltage;
  CurrentGraph::buildPixelMaps(width, false, pixelsPerSecond, newest, oldest,
                               frames, current, voltage);

  QCOMPARE(current.size(), static_cast<std::size_t>(width));
  QCOMPARE(current[width - 1], 2.0);
}

void TestGraphMapping::linearMappingLeavesNullSlotsForMissingData() {
  constexpr int width = 100;
  constexpr double pixelsPerSecond = 4.0;
  const double msPerPixel = 1000.0 / pixelsPerSecond;

  const qint64 newest = 1000;
  const qint64 oldest = newest - static_cast<qint64>(width * msPerPixel);

  // Only one frame in the rightmost pixel.
  std::vector<DisplayFrame> frames;
  DisplayFrame f;
  f.timestampMs = newest - static_cast<qint64>(msPerPixel / 2);
  f.voltage = 5.0;
  f.current = 1.0;
  f.power = 5.0;
  frames.push_back(f);

  std::vector<double> current;
  std::vector<double> voltage;
  CurrentGraph::buildPixelMaps(width, false, pixelsPerSecond, newest, oldest,
                               frames, current, voltage);

  QCOMPARE(current.size(), static_cast<std::size_t>(width));
  QCOMPARE(current[width - 1], 1.0);
  QVERIFY(current[0] == 0.0); // oldest pixel has no data
}

void TestGraphMapping::logMappingIsStableUnderSubPixelTimeAdvance() {
  constexpr int width = 400;
  constexpr double pixelsPerSecond = 4.0;

  auto frames = makeConstantFrames(100, 0, 25);

  const qint64 newest1 = 2500;

  std::vector<double> current1;
  std::vector<double> voltage1;
  CurrentGraph::buildPixelMaps(width, true, pixelsPerSecond, newest1, 0,
                               frames, current1, voltage1);

  const qint64 newest2 = newest1 + 50;

  std::vector<double> current2;
  std::vector<double> voltage2;
  CurrentGraph::buildPixelMaps(width, true, pixelsPerSecond, newest2, 0,
                               frames, current2, voltage2);

  QCOMPARE(current1.size(), static_cast<std::size_t>(width));
  QCOMPARE(current2.size(), static_cast<std::size_t>(width));

  for (int i = 0; i < width; ++i) {
    QCOMPARE(current1[i], current2[i]);
    QCOMPARE(voltage1[i], voltage2[i]);
  }
}
