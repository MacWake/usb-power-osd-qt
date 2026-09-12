#ifndef TEST_GRAPH_CACHE_H
#define TEST_GRAPH_CACHE_H

#include <QObject>

class TestGraphCache : public QObject {
  Q_OBJECT

private slots:
  // Allocation and ordering
  void oneEntryPerPixel();
  void newestFirstOrder();

  // Resize
  void resizePreservesNewestData();
  void resizeAddsNullSlotsOnGrowth();

  // History/rebuild mode (linear only)
  void historyRebuildIsLinear();
  void historyRebuildClearsOldData();

  // Live linear mode
  void liveLinearAppendsNewestAtIndexZero();
  void liveLinearShiftsOlderEntriesRight();
  void liveLinearScrollsByWholePixels();
  void liveLinearLeavesNullSlots();
  void liveLinearKeepsMaxCurrentPerPixel();

  // Stats
  void statsComputedFromEntries();

  // Logarithmic mode
  void logRebuildFillsAllPixels();
  void logBinsFramesByAge();
  void logLiveUpdateIsStableForSubPixelAdvance();
  void logInvalidEntriesDoNotDraw();

  // Live linear mode should not overwrite the rightmost pixel during sub-pixel
  // view advances, to avoid drawing jitter over the existing newest value.
  void liveLinearPreservesNewestPixelUntilScroll();
};

#endif // TEST_GRAPH_CACHE_H
