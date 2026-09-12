#include "test_serial_to_graph.h"

#include "GraphCache.h"
#include "MeasurementPipeline.h"
#include "PLDAdapter.h"
#include "PowerData.h"

#include <QByteArray>
#include <QString>
#include <QTest>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {

// Minimal LCG so the jitter is deterministic across platforms.
class DeterministicRng {
public:
  explicit DeterministicRng(uint32_t seed = 12345u) : m_state(seed) {}

  // Returns integer in [0, max].
  uint32_t next(uint32_t max) {
    m_state = m_state * 1103515245u + 12345u;
    return max == 0 ? 0 : (m_state >> 1) % (max + 1);
  }

private:
  uint32_t m_state;
};

// Encode a voltage/current pair into a PLD28 line (9 bytes: 8 hex digits +
// frame-type byte). PLD28 fits the 4 A / 20 V ramp; PLD20 would overflow its
// 4-hex-digit shunt value.
QByteArray encodePld28(double voltage, double current) {
  // PLD28: shunt (first 4 hex chars) = current / 0.2, bus (next 4) = voltage * 320.
  const int shunt = static_cast<int>(std::round(current * 1000.0 / 0.2));
  const int bus = static_cast<int>(std::round(voltage * 320.0));
  const QString shuntHex =
      QStringLiteral("%1").arg(std::max(0, shunt), 4, 16, QLatin1Char('0'));
  const QString busHex =
      QStringLiteral("%1").arg(std::max(0, bus), 4, 16, QLatin1Char('0'));
  QByteArray line = (shuntHex + busHex).toLatin1().toUpper();
  line.append(static_cast<char>(28));
  return line;
}

bool parsePldLine(const QByteArray &line, PowerData &out) {
  PLDAdapter::FrameType type = PLDAdapter::FrameType::PLD20;
  if (!PLDAdapter::detectPLDProtocol(line, type)) {
    return false;
  }
  return PLDAdapter::parsePLDLine(line, type, out);
}

struct RampSample {
  double voltage;
  double current;
  qint64 deviceTimestampMs; // relative to first sample
};

struct GeneratedRamp {
  std::vector<RampSample> samples;
};

// Build ~10 s of PLD28 data at roughly 30 Hz (with up to 5 ms of extra pause).
// The ramp goes from startV/startA to endV/endA linearly in device time.
GeneratedRamp generateRamp(double startV, double startA, double endV,
                           double endA, qint64 durationMs,
                           qint64 baseIntervalMs, qint64 maxJitterMs,
                           uint32_t seed = 42u) {
  GeneratedRamp ramp;

  DeterministicRng rng(seed);
  qint64 ts = 0;

  const int expectedSamples =
      static_cast<int>(durationMs / baseIntervalMs) + 2;
  ramp.samples.reserve(expectedSamples);

  int index = 0;
  while (ts <= durationMs) {
    const double t = static_cast<double>(ts) / static_cast<double>(durationMs);
    const double voltage = startV + (endV - startV) * t;
    const double current = startA + (endA - startA) * t;

    ramp.samples.push_back({voltage, current, ts});

    ++index;
    const qint64 jitter =
        static_cast<qint64>(rng.next(static_cast<uint32_t>(maxJitterMs)));
    ts = static_cast<qint64>(index) * baseIntervalMs + jitter;
  }

  return ramp;
}

// Build ~10 s of PLD28 data at roughly 30 Hz with up to 5 ms of jitter.
// Voltage is held constant; the current toggles between lowA and highA each
// sample. This lets us verify that rapid current jumps are preserved in the
// normalized pipeline data and graph cache.
GeneratedRamp generateJumpRamp(double voltage, double lowA, double highA,
                               qint64 durationMs, qint64 baseIntervalMs,
                               qint64 maxJitterMs, uint32_t seed = 43u) {
  GeneratedRamp ramp;

  DeterministicRng rng(seed);
  qint64 ts = 0;

  const int expectedSamples =
      static_cast<int>(durationMs / baseIntervalMs) + 2;
  ramp.samples.reserve(expectedSamples);

  int index = 0;
  int toggle = 0;
  while (ts <= durationMs) {
    const double current = (toggle % 2 == 0) ? lowA : highA;
    ramp.samples.push_back({voltage, current, ts});

    ++toggle;
    ++index;
    const qint64 jitter =
        static_cast<qint64>(rng.next(static_cast<uint32_t>(maxJitterMs)));
    ts = static_cast<qint64>(index) * baseIntervalMs + jitter;
  }

  return ramp;
}

bool entryIsValid(const GraphCache::Entry &e) { return e.voltage >= 0.0; }

} // namespace

void TestSerialToGraph::initTestCase() {}
void TestSerialToGraph::cleanupTestCase() {}

void TestSerialToGraph::rampThroughPipelineAndLinearCache() {
  MeasurementPipeline pipeline;
  pipeline.setFrameRate(30);
  pipeline.setMinCurrentThreshold(0.0f);

  constexpr qint64 durationMs = 10000;
  constexpr qint64 baseIntervalMs = 1000 / 30; // ~33 ms
  constexpr qint64 maxJitterMs = 5;

  const GeneratedRamp ramp =
      generateRamp(5.0, 0.01, 20.0, 4.0, durationMs, baseIntervalMs, maxJitterMs);

  QVERIFY(ramp.samples.size() >= 280);
  QVERIFY(ramp.samples.size() <= 330);

  // Feed the generated PLD28 lines through the serial parser and into the
  // measurement pipeline, exactly as if they arrived from the serial port.
  for (const auto &sample : ramp.samples) {
    const QByteArray line = encodePld28(sample.voltage, sample.current);
    PowerData data;
    QVERIFY2(parsePldLine(line, data),
             "Generated PLD28 line should always parse");

    // Replace the wall-clock timestamp with deterministic device time so the
    // pipeline preserves the 30 Hz + jitter spacing.
    data.timestamp = static_cast<uint64_t>(sample.deviceTimestampMs);
    data.power = data.voltage * data.current;
    data.energy = 0.0;

    pipeline.pushSample(data);
  }

  const DisplayFrame latest = pipeline.latestFrame();
  QVERIFY(latest.sampleCount > 0);

  // The pipeline should preserve the ramp: current and voltage are near the
  // top of the ramp after 10 seconds.
  QVERIFY(latest.voltage >= 19.5);
  QVERIFY(latest.voltage <= 21.0);
  QVERIFY(latest.current >= 3.8);
  QVERIFY(latest.current <= 4.2);

  // Build a frame list and feed it to the linear cache.
  const int graphWidth = 300;
  const qint64 viewRightMs = latest.timestampMs;
  const qint64 viewLeftMs = viewRightMs - (graphWidth * 1000 / 30);
  const auto frames = pipeline.framesInRange(viewLeftMs, viewRightMs);

  QVERIFY(!frames.empty());
  QVERIFY(frames.front().timestampMs <= frames.back().timestampMs);

  GraphCache cache;
  cache.setSize(graphWidth);
  cache.setParams(false, 30.0, true);
  cache.rebuild(frames, viewRightMs);

  // The newest pixel should have the latest (highest) values.
  QVERIFY(entryIsValid(cache.entries()[0]));
  QVERIFY(cache.entries()[0].current >= 3.8);
  QVERIFY(cache.entries()[0].voltage >= 19.5);

  // The oldest populated pixel should be near the start of the ramp.
  int lastValid = -1;
  for (int i = 0; i < graphWidth; ++i) {
    if (entryIsValid(cache.entries()[i])) {
      lastValid = i;
    }
  }
  QVERIFY(lastValid > 0);
  QVERIFY(lastValid < graphWidth);
  QVERIFY(cache.entries()[lastValid].current <= 1.0);
  QVERIFY(cache.entries()[lastValid].voltage <= 8.0);

  // Stats should span the whole ramp.
  QVERIFY(cache.hasData());
  QVERIFY(cache.minCurrent() <= 0.1);
  QVERIFY(cache.maxCurrent() >= 3.8);
}

void TestSerialToGraph::rampThroughPipelineAndLogCache() {
  MeasurementPipeline pipeline;
  pipeline.setFrameRate(30);
  pipeline.setMinCurrentThreshold(0.0f);

  constexpr qint64 durationMs = 10000;
  constexpr qint64 baseIntervalMs = 1000 / 30;
  constexpr qint64 maxJitterMs = 5;

  const GeneratedRamp ramp =
      generateRamp(5.0, 0.01, 20.0, 4.0, durationMs, baseIntervalMs, maxJitterMs);

  for (const auto &sample : ramp.samples) {
    const QByteArray line = encodePld28(sample.voltage, sample.current);
    PowerData data;
    QVERIFY2(parsePldLine(line, data),
             "Generated PLD28 line should always parse");

    data.timestamp = static_cast<uint64_t>(sample.deviceTimestampMs);
    data.power = data.voltage * data.current;
    data.energy = 0.0;

    pipeline.pushSample(data);
  }

  const DisplayFrame latest = pipeline.latestFrame();
  QVERIFY(latest.sampleCount > 0);

  const int graphWidth = 300;
  const qint64 viewRightMs = latest.timestampMs;
  const auto frames = pipeline.framesInRange(viewRightMs - 12000, viewRightMs);

  GraphCache cache;
  cache.setSize(graphWidth);
  cache.setParams(true, 30.0, true);
  cache.rebuild(frames, viewRightMs);

  // Newest high-resolution pixels contain the latest data.
  QVERIFY(entryIsValid(cache.entries()[0]));
  QVERIFY(cache.entries()[0].current >= 3.8);

  // No phantom data should appear at the far left beyond the actual ramp age.
  int invalidFarLeft = 0;
  for (int i = graphWidth - 30; i < graphWidth; ++i) {
    if (!entryIsValid(cache.entries()[i])) {
      ++invalidFarLeft;
    }
  }
  QVERIFY2(invalidFarLeft > 0,
           "Far-left pixels beyond the real data age should stay invalid");

  // The overall cache should still contain the ramp extremes.
  QVERIFY(cache.hasData());
  QVERIFY(cache.maxCurrent() >= 3.8);
  QVERIFY(cache.minCurrent() <= 0.5);
}

namespace {

void pushRampThroughPipeline(MeasurementPipeline &pipeline,
                            const GeneratedRamp &ramp) {
  for (const auto &sample : ramp.samples) {
    const QByteArray line = encodePld28(sample.voltage, sample.current);
    PowerData data;
    QVERIFY2(parsePldLine(line, data),
             "Generated PLD28 line should always parse");

    // Use deterministic device-relative timestamps so the test is stable and
    // independent of the host clock.
    data.timestamp = static_cast<uint64_t>(sample.deviceTimestampMs);
    data.power = data.voltage * data.current;
    data.energy = 0.0;

    pipeline.pushSample(data);
  }
}

} // namespace

void TestSerialToGraph::rampPipelineTimestampsAreMonotonicAt30Hz() {
  MeasurementPipeline pipeline;
  pipeline.setFrameRate(30);
  pipeline.setMinCurrentThreshold(0.0f);

  constexpr qint64 durationMs = 10000;
  constexpr qint64 baseIntervalMs = 1000 / 30;
  constexpr qint64 maxJitterMs = 5;

  const GeneratedRamp ramp =
      generateRamp(5.0, 0.01, 20.0, 4.0, durationMs, baseIntervalMs, maxJitterMs);
  pushRampThroughPipeline(pipeline, ramp);

  const auto frames = pipeline.framesForDuration(20.0);
  QVERIFY(frames.size() >= 280);

  // Timestamps must be strictly increasing. Any inversion is a pipeline bug.
  for (std::size_t i = 1; i < frames.size(); ++i) {
    QVERIFY2(frames[i].timestampMs > frames[i - 1].timestampMs,
             "Pipeline DisplayFrame timestamps must be strictly monotonic");
  }

  // The source jitter is at most 5 ms, so consecutive 33 ms buckets should
  // never contain more than a few duplicate source samples. With a 30 Hz frame
  // rate the expected spacing between buckets is ~33 ms.
  for (std::size_t i = 1; i < frames.size(); ++i) {
    const qint64 delta = frames[i].timestampMs - frames[i - 1].timestampMs;
    QVERIFY2(delta > 0 && delta <= 50,
             "Consecutive pipeline buckets should be spaced by one frame interval");
  }
}

void TestSerialToGraph::rapidCurrentJumpsSurvivePipeline() {
  MeasurementPipeline pipeline;
  pipeline.setFrameRate(30);
  pipeline.setMinCurrentThreshold(0.0f);

  constexpr qint64 durationMs = 10000;
  constexpr qint64 baseIntervalMs = 1000 / 30;
  constexpr qint64 maxJitterMs = 5;

  // 20 V constant, current toggles 1 A / 4 A each sample.
  const GeneratedRamp ramp =
      generateJumpRamp(20.0, 1.0, 4.0, durationMs, baseIntervalMs, maxJitterMs);
  pushRampThroughPipeline(pipeline, ramp);

  const auto frames = pipeline.framesForDuration(20.0);
  QVERIFY(frames.size() >= 280);

  int consecutiveSame = 0;
  int maxConsecutiveSame = 0;
  int lowCount = 0;
  int highCount = 0;

  for (const auto &frame : frames) {
    QVERIFY(std::abs(frame.voltage - 20.0) <= 0.1);
    QVERIFY(frame.current >= 0.9 && frame.current <= 4.1);

    if (frame.current < 2.5) {
      ++lowCount;
      if (consecutiveSame > 0) {
        maxConsecutiveSame = std::max(maxConsecutiveSame, consecutiveSame);
      }
      consecutiveSame = 1;
    } else {
      ++highCount;
      if (consecutiveSame < 0) {
        maxConsecutiveSame = std::max(maxConsecutiveSame, -consecutiveSame);
      }
      consecutiveSame = -1;
    }

    if (consecutiveSame > 0) {
      maxConsecutiveSame = std::max(maxConsecutiveSame, consecutiveSame);
    } else {
      maxConsecutiveSame = std::max(maxConsecutiveSame, -consecutiveSame);
    }
  }

  QVERIFY(lowCount > 0);
  QVERIFY(highCount > 0);
  QVERIFY2(std::abs(lowCount - highCount) <= 20,
           "Low and high current buckets should appear in roughly equal numbers");

  // With up to 5 ms jitter around a 33 ms grid, at most a few consecutive
  // source samples can land in the same bucket. Tolerate up to 10 identical
  // values in a row.
  QVERIFY2(maxConsecutiveSame <= 10,
           "Pipeline should not collapse rapid 1 A / 4 A jumps into long runs");

  // Both levels must be present in the normalized data.
  bool sawLow = false;
  bool sawHigh = false;
  for (const auto &frame : frames) {
    if (frame.current < 2.5) {
      sawLow = true;
    } else {
      sawHigh = true;
    }
  }
  QVERIFY(sawLow);
  QVERIFY(sawHigh);
}
