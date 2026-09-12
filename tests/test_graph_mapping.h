#ifndef TEST_GRAPH_MAPPING_H
#define TEST_GRAPH_MAPPING_H

#include <QObject>

class TestGraphMapping : public QObject {
  Q_OBJECT

private slots:
  void linearMappingIsStableUnderSubPixelTimeAdvance();
  void linearMappingScrollsByWholePixel();
  void linearMappingKeepsMaxCurrentPerPixel();
  void linearMappingLeavesNullSlotsForMissingData();
  void logMappingIsStableUnderSubPixelTimeAdvance();
};

#endif // TEST_GRAPH_MAPPING_H
