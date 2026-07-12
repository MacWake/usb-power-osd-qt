#include "test_serial_pld_protocol.h"

#include "PowerData.h"
#include "SerialManager.h"

#include <QTest>

void TestSerialPldProtocol::detectPld20EightByteLine() {
  SerialProtocol protocol = SerialProtocol::PLD28; // start with wrong value
  QVERIFY(SerialManager::detectPLDProtocol("12345678", protocol));
  QCOMPARE(protocol, SerialProtocol::PLD20);
}

void TestSerialPldProtocol::detectPld20WithFrameType() {
  QByteArray line = "12345678";
  line.append(static_cast<char>(20));
  SerialProtocol protocol = SerialProtocol::PLD28;
  QVERIFY(SerialManager::detectPLDProtocol(line, protocol));
  QCOMPARE(protocol, SerialProtocol::PLD20);
}

void TestSerialPldProtocol::detectPld28WithFrameType() {
  QByteArray line = "12345678";
  line.append(static_cast<char>(28));
  SerialProtocol protocol = SerialProtocol::PLD20;
  QVERIFY(SerialManager::detectPLDProtocol(line, protocol));
  QCOMPARE(protocol, SerialProtocol::PLD28);
}

void TestSerialPldProtocol::detectInvalidLines() {
  SerialProtocol protocol = SerialProtocol::PLD20;
  QVERIFY(!SerialManager::detectPLDProtocol("1234567", protocol)); // too short
  QVERIFY(!SerialManager::detectPLDProtocol("", protocol));

  QByteArray line = "12345678";
  line.append(static_cast<char>(99));
  QVERIFY(!SerialManager::detectPLDProtocol(line, protocol));
}

void TestSerialPldProtocol::parsePld20Line() {
  // PLD20 line layout: first 4 hex chars = shunt, next 4 = bus.
  // shunt 0x61A8 = 25000 -> 1.5 A, bus 0x2710 = 10000 -> 5.0 V
  PowerData d;
  QVERIFY(SerialManager::parsePLDLine("61A82710", SerialProtocol::PLD20, d));
  QCOMPARE(d.voltage, 5.0);
  QCOMPARE(d.current, 1.5);
  QCOMPARE(d.power, 7.5);
  QVERIFY(d.timestamp != 0);
}

void TestSerialPldProtocol::parsePld28Line() {
  // PLD28 line layout: first 4 hex chars = shunt, next 4 = bus.
  // shunt 0x1D4C = 7500 -> 1.5 A, bus 0x0640 = 1600 -> 5.0 V
  PowerData d;
  QVERIFY(SerialManager::parsePLDLine("1D4C0640", SerialProtocol::PLD28, d));
  QCOMPARE(d.voltage, 5.0);
  QCOMPARE(d.current, 1.5);
  QCOMPARE(d.power, 7.5);
  QVERIFY(d.timestamp != 0);
}

void TestSerialPldProtocol::parsePld20WithFrameTypeByte() {
  QByteArray line = "61A82710";
  line.append(static_cast<char>(20));
  PowerData d;
  QVERIFY(SerialManager::parsePLDLine(line, SerialProtocol::PLD20, d));
  QCOMPARE(d.voltage, 5.0);
  QCOMPARE(d.current, 1.5);
}

void TestSerialPldProtocol::parseLineTooShortFails() {
  PowerData d;
  QVERIFY(!SerialManager::parsePLDLine("1234567", SerialProtocol::PLD20, d));
}

void TestSerialPldProtocol::parseInvalidProtocolFails() {
  PowerData d;
  QVERIFY(!SerialManager::parsePLDLine("12345678",
                                         static_cast<SerialProtocol>(123), d));
}
