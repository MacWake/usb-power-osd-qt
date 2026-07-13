#include "SerialManager.h"

#include "OsdSettings.h"
#include "PLDAdapter.h"
#include "PowerData.h"

#include <QDebug>
#include <QException>
#include <QThread>

SerialManager::SerialManager(QObject *parent)
    : PowerDataSource(parent), m_serialPort(new QSerialPort(this)) {
  connect(m_serialPort, &QSerialPort::readyRead, this,
          &SerialManager::onSerialDataReady);
  connect(m_serialPort, &QSerialPort::errorOccurred, this,
          &SerialManager::onSerialError);

  // Register available protocol adapters.
  m_adapters.push_back(std::make_unique<PLDAdapter>());
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
  m_currentAdapter = nullptr;

  m_serialPort->setPort(portInfo);
  m_serialPort->setDataBits(QSerialPort::Data8);
  m_serialPort->setParity(QSerialPort::NoParity);
  m_serialPort->setStopBits(QSerialPort::OneStop);
  m_serialPort->setFlowControl(QSerialPort::NoFlowControl);

  // Try 115200 baud first.
  qDebug() << "Trying 115200 baud for serial device:" << portInfo.systemLocation()
           << "(Name:" << portInfo.portName() << ", Null:" << portInfo.isNull()
           << ")";
  if (m_serialPort->open(QIODevice::ReadWrite)) {
    m_serialPort->setBaudRate(QSerialPort::Baud115200);
    if (m_serialPort->isReadable() && m_serialPort->isWritable() && tryAdapters()) {
      m_isConnected = true;
      emit connected(portInfo.systemLocation());
      qDebug() << "Connected to serial device type" << m_currentAdapter->name()
               << "at 115200:" << portInfo.systemLocation();
      return true;
    }
    m_serialPort->close();
  }

  // Try 9600 baud (fallback).
  qDebug() << "Trying 9600 baud for serial device:" << portInfo.systemLocation()
           << "(Name:" << portInfo.portName() << ", Null:" << portInfo.isNull()
           << ")";
  if (m_serialPort->open(QIODevice::ReadWrite)) {
    m_serialPort->setBaudRate(QSerialPort::Baud9600);
    if (m_serialPort->isReadable() && m_serialPort->isWritable() && tryAdapters()) {
      m_isConnected = true;
      emit connected(portInfo.systemLocation());
      qDebug() << "Connected to serial device type" << m_currentAdapter->name()
               << "at 9600:" << portInfo.systemLocation();
      return true;
    }
    m_serialPort->close();
  }

  qDebug() << "Failed to detect protocol on serial device:" << portInfo.systemLocation();
  return false;
}

bool SerialManager::tryAdapters() {
  for (auto &adapter : m_adapters) {
    if (adapter->detect(m_serialPort)) {
      if (adapter->init(m_serialPort)) {
        m_currentAdapter = adapter.get();
        return true;
      }
      qDebug() << "Adapter" << adapter->name()
               << "detected but init failed";
    }
  }
  return false;
}

void SerialManager::onSerialDataReady() {
  if (!m_isConnected || !m_currentAdapter) {
    return;
  }

  while (m_serialPort->canReadLine()) {
    QByteArray line = m_serialPort->readLine();
    if (line.endsWith('\n')) {
      line = line.trimmed();
    } else {
      continue;
    }

    PowerData sample;
    if (!m_currentAdapter->parseLine(line, sample)) {
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
  m_currentAdapter = nullptr;
  m_isConnected = false;
}

QString SerialManager::sourceName() const {
  if (!m_serialPort || QThread::currentThread() != m_serialPort->thread()) {
    return QString{};
  }
  return m_serialPort->portName();
}

bool SerialManager::isConnected() const { return m_isConnected; }

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
    qDebug() << "SerialManager::tryConnect: Port not found in availablePorts, "
                  "falling back to constructor for"
               << portName;
    targetPort = QSerialPortInfo(portName);
  }

  return this->connectSerialDevice(targetPort);
}
