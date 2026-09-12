#include "test_measurement_pipeline.h"

#include "MeasurementPipeline.h"
#include "PowerData.h"

#include <QDateTime>
#include <QTest>

void TestMeasurementPipeline::frameBucketingKeepsMaxCurrentAndPower() {
  MeasurementPipeline pipeline;
  pipeline.setFrameRate(30);
  pipeline.setMinCurrentThreshold(0.0f);

  const qint64 now = QDateTime::currentMSecsSinceEpoch();

  PowerData low;
  low.voltage = 5.0;
  low.current = 1.0;
  low.power = 5.0;
  low.timestamp = now;
  pipeline.pushSample(low);

  PowerData high;
  high.voltage = 5.1;
  high.current = 2.0;
  high.power = 10.2;
  high.timestamp = now;
  pipeline.pushSample(high);

  PowerData latest;
  latest.voltage = 5.2;
  latest.current = 1.5;
  latest.power = 7.8;
  latest.timestamp = now;
  pipeline.pushSample(latest);

  const DisplayFrame frame = pipeline.latestFrame();
  QCOMPARE(frame.current, 2.0); // max across bucket
  QCOMPARE(frame.power, 10.2);  // max across bucket
  QCOMPARE(frame.voltage, 5.2); // latest value wins
}

void TestMeasurementPipeline::rawWindowMaxValues() {
  MeasurementPipeline pipeline;
  pipeline.setMinCurrentThreshold(0.0f);

  PowerData s1;
  s1.voltage = 5.0;
  s1.current = 1.0;
  s1.power = 5.0;
  s1.timestamp = 1;
  pipeline.pushSample(s1);

  PowerData s2;
  s2.voltage = 5.1;
  s2.current = 2.0;
  s2.power = 10.2;
  s2.timestamp = 2;
  pipeline.pushSample(s2);

  double voltage = 0.0;
  double current = 0.0;
  double power = 0.0;
  QVERIFY(pipeline.maxValuesRawLastN(10, voltage, current, power));
  QCOMPARE(voltage, 5.1);
  QCOMPARE(current, 2.0);
  QCOMPARE(power, 10.2);
}

void TestMeasurementPipeline::pauseDetectionWhenCurrentStaysLow() {
  MeasurementPipeline pipeline;
  pipeline.setFrameRate(30);
  pipeline.setMinCurrentThreshold(0.1f); // 100 mA
  pipeline.setPausedThresholdMs(50);

  PowerData valid;
  valid.voltage = 5.0;
  valid.current = 1.0;
  valid.power = 5.0;
  valid.timestamp = QDateTime::currentMSecsSinceEpoch();
  pipeline.pushSample(valid);

  QVERIFY(!pipeline.isPaused());

  PowerData invalid;
  invalid.voltage = 5.0;
  invalid.current = 0.01; // below threshold
  invalid.power = 0.05;
  invalid.timestamp = QDateTime::currentMSecsSinceEpoch();
  pipeline.pushSample(invalid);

  QTest::qWait(100); // longer than the 50 ms pause threshold

  // Push another sub-threshold sample; the pause guard should trip.
  pipeline.pushSample(invalid);
  QVERIFY(pipeline.isPaused());
}

void TestMeasurementPipeline::deviceTimestampMappedToWallClock() {
  MeasurementPipeline pipeline;
  pipeline.setFrameRate(30);
  pipeline.setMinCurrentThreshold(0.0f);

  const qint64 wallBefore = QDateTime::currentMSecsSinceEpoch();

  PowerData sample;
  sample.voltage = 5.0;
  sample.current = 1.0;
  sample.power = 5.0;
  sample.timestamp = 1000; // small device-relative timestamp
  pipeline.pushSample(sample);

  const DisplayFrame frame = pipeline.latestFrame();
  const qint64 wallAfter = QDateTime::currentMSecsSinceEpoch();

  // The frame timestamp is quantized to the frame interval, so allow a
  // one-bucket margin on either side of the wall-clock window.
  const qint64 interval = pipeline.frameIntervalMs();
  QVERIFY(frame.timestampMs >= wallBefore - interval);
  QVERIFY(frame.timestampMs <= wallAfter + interval);
}

void TestMeasurementPipeline::forwardFillsEmptyBuckets() {
  MeasurementPipeline pipeline;
  pipeline.setFrameRate(10); // 100 ms buckets for easier verification
  pipeline.setMinCurrentThreshold(0.0f);

  const qint64 interval = pipeline.frameIntervalMs();
  const qint64 now =
      (QDateTime::currentMSecsSinceEpoch() / interval) * interval;

  PowerData first;
  first.voltage = 5.0;
  first.current = 1.0;
  first.power = 5.0;
  first.timestamp = now;
  pipeline.pushSample(first);

  PowerData later;
  later.voltage = 5.0;
  later.current = 2.0;
  later.power = 10.0;
  later.timestamp = now + 3 * interval;
  pipeline.pushSample(later);

  const auto frames = pipeline.framesForDuration(1.0);
  QVERIFY(!frames.empty());

  bool foundFilledBucket = false;
  for (const auto &frame : frames) {
    if (frame.timestampMs == now + interval ||
        frame.timestampMs == now + 2 * interval) {
      QCOMPARE(frame.current, 1.0); // forward-filled from the first sample
      foundFilledBucket = true;
    }
  }
  QVERIFY(foundFilledBucket);
}
