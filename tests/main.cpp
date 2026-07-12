#include "test_measurement_pipeline.h"
#include "test_serial_pld_protocol.h"
#include "test_v2_ble_protocol.h"

#include <QTest>

int main(int argc, char *argv[]) {
  int status = 0;
  status |= QTest::qExec(new TestV2BleProtocol, argc, argv);
  status |= QTest::qExec(new TestSerialPldProtocol, argc, argv);
  status |= QTest::qExec(new TestMeasurementPipeline, argc, argv);
  return status;
}
