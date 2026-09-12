#ifndef TEST_MEASUREMENT_PIPELINE_H
#define TEST_MEASUREMENT_PIPELINE_H

#include <QObject>

class TestMeasurementPipeline : public QObject {
  Q_OBJECT

private slots:
  void frameBucketingKeepsMaxCurrentAndPower();
  void rawWindowMaxValues();
  void pauseDetectionWhenCurrentStaysLow();
  void deviceTimestampMappedToWallClock();
  void forwardFillsEmptyBuckets();
};

#endif // TEST_MEASUREMENT_PIPELINE_H
