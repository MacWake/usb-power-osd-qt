#ifndef TEST_V2_BLE_PROTOCOL_H
#define TEST_V2_BLE_PROTOCOL_H

#include <QObject>

class TestV2BleProtocol : public QObject {
  Q_OBJECT

private slots:
  void jsonParseFullPayload();
  void jsonParseMissingFieldsUseDefaults();
  void jsonParseTimestampFallback();
  void binaryPacketParsesVoltageCurrentPower();
  void binaryPacketTooShortIsIgnored();
};

#endif // TEST_V2_BLE_PROTOCOL_H
