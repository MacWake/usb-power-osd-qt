#ifndef TEST_SERIAL_TO_GRAPH_H
#define TEST_SERIAL_TO_GRAPH_H

#include <QObject>

class TestSerialToGraph : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();

  // Simulate ~10 s of PLD28 serial data: 5 V / 10 mA to 20 V / 4 A,
  // roughly 30 Hz with up to 5 ms of jitter, then push it through the
  // measurement pipeline and graph cache.
  void rampThroughPipelineAndLinearCache();
  void rampThroughPipelineAndLogCache();

  // Verify that pipeline DisplayFrame timestamps are monotonic and lie on a
  // 30 Hz grid even when source samples arrive with jitter.
  void rampPipelineTimestampsAreMonotonicAt30Hz();

  // Rapid current toggles (1 A / 4 A at 20 V) must survive normalization.
  void rapidCurrentJumpsSurvivePipeline();

  void cleanupTestCase();
};

#endif // TEST_SERIAL_TO_GRAPH_H
