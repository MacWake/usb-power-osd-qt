#include "SerialManager.h"

#include "OsdSettings.h"
#include "PowerData.h"

#include <QDebug>
#include <QThread>
#include <QElapsedTimer>
#include <iostream>
#include <ostream>

#include <QtCore/qcoreapplication.h>
#include <QtCore/qdatetime.h>
#include <QException>
#include "hexdump.h"

// These should match your USB Power OSD V2 device VID/PID
const quint16 SerialManager::TARGET_VENDOR_ID = 0x0483;  // STMicroelectronics
const quint16 SerialManager::TARGET_PRODUCT_ID = 0x5740; // Virtual COM Port

static int8_t hex2bin(const unsigned char c) {
  if (c >= '0' && c <= '9') {
    return c - '0'; // NOLINT(*-narrowing-conversions)
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10; // NOLINT(*-narrowing-conversions)
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10; // NOLINT(*-narrowing-conversions)
  }
  return 0;
}

static int16_t hex4_to_uint16(const char *buf) {
  const int16_t val =
      (hex2bin(buf[0]) << 12) | // NOLINT(*-narrowing-conversions)
      (hex2bin(buf[1]) << 8) |  // NOLINT(*-narrowing-conversions)
      (hex2bin(buf[2]) << 4) |  // NOLINT(*-narrowing-conversions)
      hex2bin(buf[3]);
  return val;
}
static int16_t hex4_to_int16(const char *buf) {
  return static_cast<int16_t>(strtol(buf, nullptr, 16));
}

SerialManager::SerialManager(QObject *parent)
    : PowerDataSource(parent), m_serialPort(new QSerialPort(this)) {
  connect(m_serialPort, &QSerialPort::readyRead, this,
          &SerialManager::onSerialDataReady);
  connect(m_serialPort, &QSerialPort::errorOccurred, this,
          &SerialManager::onSerialError);
}

SerialManager::~SerialManager() {
  if (m_serialPort->isOpen()) {
    m_serialPort->close();
  }
}

bool SerialManager::connectSerialDevice(const QSerialPortInfo &portInfo) {
  if (portInfo.isNull()) {
    qDebug() << "SerialManager::connectSerialDevice: portInfo is null!";
    return false;
  }

  if (m_serialPort->isOpen()) {
    m_serialPort->close();
  }

  m_serialPort->setPort(portInfo);
  m_serialPort->setDataBits(QSerialPort::Data8);
  m_serialPort->setParity(QSerialPort::NoParity);
  m_serialPort->setStopBits(QSerialPort::OneStop);
  m_serialPort->setFlowControl(QSerialPort::NoFlowControl);

  // Try 115200 baud first
  qDebug() << "Trying 115200 baud for serial device:" << portInfo.systemLocation()
           << "(Name:" << portInfo.portName() << ", Null:" << portInfo.isNull() << ")";
  if (m_serialPort->open(QIODevice::ReadWrite)) {
    m_serialPort->setBaudRate(QSerialPort::Baud115200);
    if (m_serialPort->isReadable() && m_serialPort->isWritable()) {
      if (this->checkPLDProtocol()) {
        m_isConnected = true;
        emit connected(portInfo.systemLocation());
        qDebug() << "Connected to serial device type " << this->m_protocol << " at 115200:" << portInfo.systemLocation();
        return true;
      }
      if (this->checkMacwakeProtocol()) {
        m_isConnected = true;
        emit connected(portInfo.systemLocation());
        qDebug() << "Connected to Macwake device at 115200:" << portInfo.systemLocation();
        return true;
      }
    }
    m_serialPort->close();
  }

  // Try 9600 baud (fallback)
  qDebug() << "Trying 9600 baud for serial device:" << portInfo.systemLocation()
           << "(Name:" << portInfo.portName() << ", Null:" << portInfo.isNull() << ")";
  if (m_serialPort->open(QIODevice::ReadWrite)) {
    m_serialPort->setBaudRate(QSerialPort::Baud9600);
    if (m_serialPort->isReadable() && m_serialPort->isWritable()) {
      if (this->checkPLDProtocol()) {
        m_isConnected = true;
        emit connected(portInfo.systemLocation());
        qDebug() << "Connected to serial device type " << this->m_protocol << " at 9600:" << portInfo.systemLocation();
        return true;
      }
    }
    m_serialPort->close();
  }

  qDebug() << "Failed to detect protocol on serial device:" << portInfo.systemLocation();
  return false;
}

bool SerialManager::detectPLDProtocol(const QByteArray &line,
                                      SerialProtocol &protocol) {
  if (line.length() == 9) {
    if (line.at(8) == 28) {
      protocol = SerialProtocol::PLD28;
      return true;
    }
    if (line.at(8) == 20) {
      protocol = SerialProtocol::PLD20;
      return true;
    }
  } else if (line.length() == 8) {
    protocol = SerialProtocol::PLD20;
    return true;
  }
  return false;
}

bool SerialManager::parsePLDLine(const QByteArray &line,
                                  SerialProtocol protocol, PowerData &out) {
  double voltage_quanta;
  double current_quanta;

  if (protocol == SerialProtocol::PLD28) {
    voltage_quanta = 3.125;
    current_quanta = 0.2; // with 50mR shunt
  } else if (protocol == SerialProtocol::PLD20) {
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

  if (protocol == SerialProtocol::PLD20) {
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

void SerialManager::onSerialDataReady() {
  if (!m_isConnected) {
    return;
  }

  while (m_serialPort->canReadLine()) {
    auto line = m_serialPort->readLine();
    if (line.endsWith('\n')) {
      line = line.trimmed();
    } else {
      continue;
    }

    if (line.size() < 8 || line.size() > 11) {
      qDebug() << "Bad packet length " << line.size();
      continue;
    }

    PowerData sample;
    if (!parsePLDLine(line, m_protocol, sample)) {
      qDebug() << "Unhandled protocol " << m_protocol;
      continue;
    }

    emit sampleReceived(sample);
  }
}
void SerialManager::start() { /* no-op: serial is started via tryConnect */ }
void SerialManager::stop() { disconnect(); }

void SerialManager::disconnect() {
  qDebug() << "Disconnecting from serial device";
  try {
    if (m_serialPort->isOpen()) {
      m_serialPort->close();
    }
  } catch (QException &e) {
    qDebug() << "Exception while disconnecting: " << e.what();
  }
  m_isConnected = false;
}

QString SerialManager::sourceName() const {
  if (!m_serialPort || QThread::currentThread() != m_serialPort->thread()) {
    return QString{};
  }
  return m_serialPort->portName();
}

bool SerialManager::isConnected() const {
  return m_isConnected;
}

void SerialManager::onSerialError(QSerialPort::SerialPortError error) {
  if (error != QSerialPort::NoError) {
    qDebug() << "Serial port error:" << error;
    if (m_isConnected)
      emit disconnected();
    m_isConnected = false;
  }
}
bool SerialManager::tryConnect(const QString &portName) {
  qDebug() << "SerialManager::tryConnect: Received request for" << portName;

  if (portName.isEmpty()) {
    qDebug() << "SerialManager::tryConnect: Port name is empty";
    return false;
  }

  QSerialPortInfo targetPort;
  const auto ports = QSerialPortInfo::availablePorts();
  for (const auto &port : ports) {
    if (port.portName() == portName || port.systemLocation() == portName) {
      targetPort = port;
      break;
    }
  }

  if (targetPort.isNull()) {
    qDebug() << "SerialManager::tryConnect: Port not found in availablePorts, falling back to constructor for" << portName;
    targetPort = QSerialPortInfo(portName);
  }

  return this->connectSerialDevice(targetPort);
}

bool SerialManager::waitForLineAvailable(int timeoutMs) {
  QElapsedTimer timer;
  timer.start();
  while (timer.elapsed() < timeoutMs) {
    if (m_serialPort->canReadLine()) {
      return true;
    }
    if (m_serialPort->waitForReadyRead(10)) {
        // Data arrived, next iteration will check canReadLine
    }
  }

  return m_serialPort->canReadLine();
}

bool SerialManager::checkPLDProtocol() {
  qDebug() << "Checking for PLD protocol...";

  // Give the device a short moment to stabilize and start sending data
  QThread::msleep(100);
  m_serialPort->clear();

  QElapsedTimer timer;
  timer.start();
  int linesRead = 0;

  // Try to find a valid protocol line within a reasonable window
  while (timer.elapsed() < 3000) {
    if (!this->waitForLineAvailable(500)) {
      continue;
    }

    QByteArray line = m_serialPort->readLine();
    if (line.isEmpty()) continue;
    
    linesRead++;
    if (line.endsWith('\n')) {
      line = line.trimmed();
    }

    SerialProtocol detected;
    if (detectPLDProtocol(line, detected)) {
      m_protocol = detected;
      return true;
    }

    // Don't loop forever if we're getting lots of garbage
    if (linesRead > 20) break;
  }

  qDebug() << "checkPLDProtocol: Failed after checking" << linesRead << "lines";
  return false;
}
bool SerialManager::checkMacwakeProtocol() {
  return false; // TODO
}
