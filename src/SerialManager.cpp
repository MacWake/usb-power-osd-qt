#include "SerialManager.h"

#include "OsdSettings.h"
#include "PowerData.h"

#include <QDebug>
#include <QThread>
#include <iostream>
#include <ostream>

#include <QtCore/qcoreapplication.h>
#include <QtCore/qdatetime.h>
#include <QException>

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
    : QObject(parent), m_serialPort(new QSerialPort(this)) {
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
  if (m_serialPort->isOpen()) {
    m_serialPort->close();
  }

  m_serialPort->setPort(portInfo);
  m_serialPort->setBaudRate(QSerialPort::Baud9600);
  m_serialPort->setDataBits(QSerialPort::Data8);
  m_serialPort->setParity(QSerialPort::NoParity);
  m_serialPort->setStopBits(QSerialPort::OneStop);
  m_serialPort->setFlowControl(QSerialPort::NoFlowControl);

  // qDebug() << "Trying to connect to serial device:" << portInfo.portName();
  if (!m_serialPort->open(QIODevice::ReadWrite)) {
    // qDebug() << "Failed to open serial port with 9600:" <<
    // m_serialPort->errorString();
    return false;
  }
  // qDebug() << "Opened serial port with 9600:" << portInfo.portName();
  //  Test basic functionality
  if (!m_serialPort->isWritable() || !m_serialPort->isReadable()) {
    qDebug() << "Serial port opened but has limited functionality";
    m_serialPort->close();
    return false;
  }

  if (this->checkPLDProtocol()) {
    m_isConnected = true;
    emit deviceConnected(portInfo.portName());
    // qDebug() << "Connected to serial device type " << this->m_protocol << ":"
    // << portInfo.portName();
    return true;
  } else {
    // qDebug() << "Cannot detect protocol with 9600 on serial device:" <<
    // portInfo.portName();
    m_serialPort->close();
    m_serialPort->setBaudRate(QSerialPort::Baud115200);
    if (!m_serialPort->open(QIODevice::ReadWrite)) {
      // qDebug() << "Failed to open serial port with 115200:" <<
      // m_serialPort->errorString();
      return false;
    }
    if (this->checkMacwakeProtocol()) {
      m_isConnected = true;
      emit deviceConnected(portInfo.portName());
      return true;
    }
    m_serialPort->close();
    return false;
  }
}

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
void SerialManager::onSerialDataReady() {
  if (!m_isConnected) {
    return;
  }
  if (!m_serialPort->canReadLine()) {
    return;
  }

  auto line = m_serialPort->readLine();
  if (line.endsWith('\n')) {
    line = line.trimmed();
  } else {
    return;
  }
  double voltage_quanta;
  double current_quanta;

  if (m_protocol == SerialProtocol::PLD28) {
    voltage_quanta = 3.125;
    current_quanta = 0.2; // with 50mR shunt
  } else if (m_protocol == SerialProtocol::PLD20) {
    voltage_quanta = 4.0;
    current_quanta = 0.06; // with 100mR shunt
  } else {
    qDebug() << "Unhandled protocol " << m_protocol;
    return;
  }
  // printf("len=%d\n", static_cast<int>(strlen(serial_buffer)));
  if (line.size() < 8 || line.size() > 11) {
    qDebug() << "Bad packet length " << line.size();
    return;
  }
  int shunt_voltage = hex4_to_int16(line.sliced(0, 4).toStdString().c_str());
  // printf("Shunt: %d\n", shunt_voltage);

  auto bus_voltage = static_cast<double>(
      hex4_to_uint16(line.sliced(4, 4).toStdString().c_str()));
  // if (frame_type == OSD_MODE_20V && (bus_voltage & 0x0001)) {
  //     std::cerr << "bad data?" << std::endl;
  //     break;
  // }

  if (m_protocol == PLD20) {
    bus_voltage /= 8.0;
  }

  int milliamps = abs(
      static_cast<int>(static_cast<double>(shunt_voltage) * current_quanta));
  int millivolts = static_cast<int>(bus_voltage * voltage_quanta);
  // printf("Millivolts: %f\n", bus_voltage * voltage_quanta);

  PowerData sample;
  sample.current = milliamps / 1000.0;
  sample.voltage = millivolts / 1000.0;
  sample.power = sample.voltage * sample.current;
  sample.timestamp = QDateTime::currentMSecsSinceEpoch();

  emit dataReceived(sample);
}
void SerialManager::onSerialError(QSerialPort::SerialPortError error) {
  if (error != QSerialPort::NoError) {
    qDebug() << "Serial port error:" << error;
    if (m_isConnected)
      emit deviceDisconnected();
    m_isConnected = false;
  }
}
bool SerialManager::tryConnect(const QString &portName) {
  QSerialPortInfo serialPortInfo(portName);
  return this->connectSerialDevice(serialPortInfo);
}

bool SerialManager::waitForLineAvailable(int timeoutMs) {
  QElapsedTimer timer;
  timer.start();
  while (timer.elapsed() < timeoutMs) {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

    if (m_serialPort->canReadLine()) {
      return true;
    }
    QThread::msleep(10);
  }

  return false;
}

bool SerialManager::checkPLDProtocol() {
  // qDebug() << "Checking for PLD protocol...";

  if (!this->waitForLineAvailable(1000)) {
    std::cerr << "checkPLDProtocol: timeout 1" << std::endl;
    return false;
  }
  QByteArray line = m_serialPort->readLine();
  // hexdump(line);
  if (line.size() < 8) {
    // qDebug() << "line size: " << line.size() << " too small, read next line";
    if (!this->waitForLineAvailable(1000)) {
      std::cerr << "checkPLDProtocol: timeout 2" << std::endl;
      return false;
    }
    line = m_serialPort->readLine();
    // hexdump(line);
  }
  if (line.size() < 8) {
    // qDebug() << "line size: " << line.size() << " still too small, failure";
    return false;
  }
  if (line.endsWith('\n')) {
    line = line.trimmed();
  }
  if (line.length() == 9) {
    if (line.at(8) == 28) {
      // qDebug() << "PLD28 detected (by byte)";
      m_protocol = SerialProtocol::PLD28;
    } else if (line.at(8) == 20) {
      // qDebug() << "PLD20 detected (by byte)";
      m_protocol = SerialProtocol::PLD20;
    }
  } else if (line.length() == 8) {
    // qDebug() << "PLD20 detected (by length)";
    m_protocol = SerialProtocol::PLD20;
  } else {
    std::cerr << "checkPLDProtocol: Cannot obtain frame type" << std::endl;
    return false;
  }
  return true;
}
bool SerialManager::checkMacwakeProtocol() {
  return false; // TODO
}
