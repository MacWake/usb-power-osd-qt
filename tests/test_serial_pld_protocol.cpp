#include "test_serial_pld_protocol.h"

#include "PLDAdapter.h"
#include "PowerData.h"

#include <QTest>

void TestSerialPldProtocol::detectPld20EightByteLine() {
  PLDAdapter::FrameType type = PLDAdapter::FrameType::PLD28; // start with wrong value
  QVERIFY(PLDAdapter::detectPLDProtocol("12345678", type));
  QCOMPARE(type, PLDAdapter::FrameType::PLD20);
}

void TestSerialPldProtocol::detectPld20WithFrameType() {
  QByteArray line = "12345678";
  line.append(static_cast<char>(20));
  PLDAdapter::FrameType type = PLDAdapter::FrameType::PLD28;
  QVERIFY(PLDAdapter::detectPLDProtocol(line, type));
  QCOMPARE(type, PLDAdapter::FrameType::PLD20);
}

void TestSerialPldProtocol::detectPld28WithFrameType() {
  QByteArray line = "12345678";
  line.append(static_cast<char>(28));
  PLDAdapter::FrameType type = PLDAdapter::FrameType::PLD20;
  QVERIFY(PLDAdapter::detectPLDProtocol(line, type));
  QCOMPARE(type, PLDAdapter::FrameType::PLD28);
}

void TestSerialPldProtocol::detectInvalidLines() {
  PLDAdapter::FrameType type = PLDAdapter::FrameType::PLD20;
  QVERIFY(!PLDAdapter::detectPLDProtocol("1234567", type)); // too short
  QVERIFY(!PLDAdapter::detectPLDProtocol("", type));

  QByteArray line = "12345678";
  line.append(static_cast<char>(99));
  QVERIFY(!PLDAdapter::detectPLDProtocol(line, type));
}

void TestSerialPldProtocol::parsePld20Line() {
  // PLD20 line layout: first 4 hex chars = shunt, next 4 = bus.
  // shunt 0x61A8 = 25000 -> 1.5 A, bus 0x2710 = 10000 -> 5.0 V
  PowerData d;
  QVERIFY(PLDAdapter::parsePLDLine("61A82710", PLDAdapter::FrameType::PLD20, d));
  QCOMPARE(d.voltage, 5.0);
  QCOMPARE(d.current, 1.5);
  QCOMPARE(d.power, 7.5);
  QVERIFY(d.timestamp != 0);
}

void TestSerialPldProtocol::parsePld28Line() {
  // PLD28 line layout: first 4 hex chars = shunt, next 4 = bus.
  // shunt 0x1D4C = 7500 -> 1.5 A, bus 0x0640 = 1600 -> 5.0 V
  PowerData d;
  QVERIFY(PLDAdapter::parsePLDLine("1D4C0640", PLDAdapter::FrameType::PLD28, d));
  QCOMPARE(d.voltage, 5.0);
  QCOMPARE(d.current, 1.5);
  QCOMPARE(d.power, 7.5);
  QVERIFY(d.timestamp != 0);
}

void TestSerialPldProtocol::parsePld20WithFrameTypeByte() {
  QByteArray line = "61A82710";
  line.append(static_cast<char>(20));
  PowerData d;
  QVERIFY(PLDAdapter::parsePLDLine(line, PLDAdapter::FrameType::PLD20, d));
  QCOMPARE(d.voltage, 5.0);
  QCOMPARE(d.current, 1.5);
}

void TestSerialPldProtocol::parseLineTooShortFails() {
  PowerData d;
  QVERIFY(!PLDAdapter::parsePLDLine("1234567", PLDAdapter::FrameType::PLD20, d));
}

void TestSerialPldProtocol::parseInvalidProtocolFails() {
  PowerData d;
  QVERIFY(!PLDAdapter::parsePLDLine(
      "12345678", static_cast<PLDAdapter::FrameType>(123), d));
}
