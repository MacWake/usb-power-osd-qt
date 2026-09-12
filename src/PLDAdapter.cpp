#include "PLDAdapter.h"

#include <QDateTime>
#include <QDebug>
#include <QElapsedTimer>
#include <QThread>

namespace {

static int8_t hex2bin(const unsigned char c) {
  if (c >= '0' && c <= '9') {
    return static_cast<int8_t>(c - '0');
  }
  if (c >= 'A' && c <= 'F') {
    return static_cast<int8_t>(c - 'A' + 10);
  }
  if (c >= 'a' && c <= 'f') {
    return static_cast<int8_t>(c - 'a' + 10);
  }
  return 0;
}

static int16_t hex4_to_uint16(const char *buf) {
  return static_cast<int16_t>(
      (hex2bin(buf[0]) << 12) | (hex2bin(buf[1]) << 8) |
      (hex2bin(buf[2]) << 4) | hex2bin(buf[3]));
}

static int16_t hex4_to_int16(const char *buf) {
  return static_cast<int16_t>(strtol(buf, nullptr, 16));
}

} // namespace

bool PLDAdapter::detectPLDProtocol(const QByteArray &line, FrameType &type) {
  if (line.length() == 9) {
    if (line.at(8) == 28) {
      type = FrameType::PLD28;
      return true;
    }
    if (line.at(8) == 20) {
      type = FrameType::PLD20;
      return true;
    }
  } else if (line.length() == 8) {
    type = FrameType::PLD20;
    return true;
  }
  return false;
}

bool PLDAdapter::parsePLDLine(const QByteArray &line, FrameType type,
                              PowerData &out) {
  double voltage_quanta;
  double current_quanta;

  if (type == FrameType::PLD28) {
    voltage_quanta = 3.125;
    current_quanta = 0.2; // with 50mR shunt
  } else if (type == FrameType::PLD20) {
    voltage_quanta = 4.0;
    current_quanta = 0.06; // with 100mR shunt
  } else {
    return false;
  }

  if (line.size() < 8) {
    return false;
  }

  const QByteArray data = line.left(8);
  const int shunt_voltage =
      hex4_to_int16(data.sliced(0, 4).toStdString().c_str());
  double bus_voltage = static_cast<double>(
      hex4_to_uint16(data.sliced(4, 4).toStdString().c_str()));

  if (type == FrameType::PLD20) {
    bus_voltage /= 8.0;
  }

  const int milliamps =
      qAbs(static_cast<int>(static_cast<double>(shunt_voltage) * current_quanta));
  const int millivolts = static_cast<int>(bus_voltage * voltage_quanta);

  out.current = milliamps / 1000.0;
  out.voltage = millivolts / 1000.0;
  out.power = out.voltage * out.current;
  out.timestamp = QDateTime::currentMSecsSinceEpoch();
  return true;
}

bool PLDAdapter::detect(QSerialPort *port) {
  qDebug() << "PLDAdapter: detecting protocol...";

  // Give the device a short moment to stabilize and start sending data.
  QThread::msleep(100);
  port->clear();

  QElapsedTimer timer;
  timer.start();
  int linesRead = 0;

  // Try to find a valid protocol line within a reasonable window.
  while (timer.elapsed() < 3000) {
    if (!waitForLineAvailable(port, 500)) {
      continue;
    }

    QByteArray line = readLineTrimmed(port);
    if (line.isEmpty()) {
      continue;
    }

    linesRead++;

    FrameType detected = FrameType::PLD20;
    if (detectPLDProtocol(line, detected)) {
      m_frameType = detected;
      qDebug() << "PLDAdapter: detected" <<
          (detected == FrameType::PLD28 ? "PLD28" : "PLD20");
      return true;
    }

    // Don't loop forever if we're getting lots of garbage.
    if (linesRead > 20) {
      break;
    }
  }

  qDebug() << "PLDAdapter: detection failed after checking" << linesRead
           << "lines";
  return false;
}

bool PLDAdapter::init(QSerialPort *port) {
  qDebug() << "PLDAdapter: init phase (no commands required)";
  (void)port;
  return true;
}

bool PLDAdapter::parseLine(const QByteArray &line, PowerData &out) {
  if (line.size() < 8 || line.size() > 11) {
    qDebug() << "PLDAdapter: bad packet length" << line.size();
    return false;
  }
  return parsePLDLine(line, m_frameType, out);
}
