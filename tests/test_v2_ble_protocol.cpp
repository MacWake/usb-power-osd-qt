#include "test_v2_ble_protocol.h"

#include "BluetoothManager.h"
#include "PowerData.h"
#include "PowerMonitor.h"

#include <QDateTime>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>

void TestV2BleProtocol::jsonParseFullPayload() {
  QJsonObject json;
  json["current"] = 1.5;
  json["voltage"] = 5.1;
  json["power"] = 7.65;
  json["charge"] = 0.0123;
  json["timestamp"] = static_cast<qint64>(1234567890LL);

  const PowerData d = BluetoothManager::parseJsonToPowerData(json);
  QCOMPARE(d.current, 1.5);
  QCOMPARE(d.voltage, 5.1);
  QCOMPARE(d.power, 7.65);
  QCOMPARE(d.energy, 0.0123);
  QCOMPARE(d.timestamp, 1234567890ULL);
}

void TestV2BleProtocol::jsonParseMissingFieldsUseDefaults() {
  const QJsonObject json;
  const PowerData d = BluetoothManager::parseJsonToPowerData(json);
  QCOMPARE(d.current, 0.0);
  QCOMPARE(d.voltage, 0.0);
  QCOMPARE(d.power, 0.0);
  QCOMPARE(d.energy, 0.0);
  QVERIFY(d.timestamp != 0); // falls back to current wall-clock time
}

void TestV2BleProtocol::jsonParseTimestampFallback() {
  QJsonObject json;
  json["current"] = 0.1;

  const qint64 before = QDateTime::currentMSecsSinceEpoch();
  const PowerData d = BluetoothManager::parseJsonToPowerData(json);
  const qint64 after = QDateTime::currentMSecsSinceEpoch();

  QVERIFY(d.timestamp >= static_cast<quint64>(before));
  QVERIFY(d.timestamp <= static_cast<quint64>(after));
}

void TestV2BleProtocol::binaryPacketParsesVoltageCurrentPower() {
  PowerMonitor monitor;
  QSignalSpy spy(&monitor, &PowerMonitor::powerDataReceived);

  // 12-byte V2-BLE packet: little-endian voltage/current at offsets 0 and 2.
  QByteArray data(12, '\0');
  // voltage = 5000 mV = 0x1388
  data[0] = 0x88;
  data[1] = 0x13;
  // current = 1500 mA = 0x05DC
  data[2] = 0xDC;
  data[3] = 0x05;

  monitor.processBLEData(data);

  QCOMPARE(spy.count(), 1);
  const PowerData d = qvariant_cast<PowerData>(spy.takeFirst().at(0));
  QCOMPARE(d.voltage, 5.0);
  QCOMPARE(d.current, 1.5);
  QCOMPARE(d.power, 7.5);
}

void TestV2BleProtocol::binaryPacketTooShortIsIgnored() {
  PowerMonitor monitor;
  QSignalSpy spy(&monitor, &PowerMonitor::powerDataReceived);
  monitor.processBLEData(QByteArray(5, '\0'));
  QCOMPARE(spy.count(), 0);
}
