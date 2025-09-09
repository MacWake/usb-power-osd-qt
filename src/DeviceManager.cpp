#include "DeviceManager.h"

#include "MainWindow.h"
#include "PowerMonitor.h"
#include <QDebug>

DeviceManager::DeviceManager(QObject *parent)
    : QObject(parent), m_bluetoothManager(new BluetoothManager(this)),
      m_serialManager(new SerialManager(this)),
      m_powerMonitor(new PowerMonitor(this)) {
  // Connect Bluetooth signals
  connect(m_bluetoothManager, &BluetoothManager::deviceConnected, this,
          &DeviceManager::onBluetoothDeviceConnected);
  connect(m_bluetoothManager, &BluetoothManager::deviceDisconnected, this,
          &DeviceManager::onBluetoothDeviceDisconnected);
  // connect(m_bluetoothManager, &BluetoothManager::dataReceived,
  //         this, &DeviceManager::onBluetoothDataReceived);

  // NEW: Connect to parsed power data from Bluetooth
  connect(m_bluetoothManager, &BluetoothManager::powerDataReceived, this,
          [this](const PowerData &data) {
            // Forward the parsed power data directly
            emit powerDataReceived(data);
          });

  // Connect Serial signals
  connect(m_serialManager, &SerialManager::deviceConnected, this,
          &DeviceManager::onSerialDeviceConnected);
  connect(m_serialManager, &SerialManager::deviceDisconnected, this,
          &DeviceManager::onSerialDeviceDisconnected);
  connect(m_serialManager, &SerialManager::dataReceived, this,
          &DeviceManager::onSerialDataReceived);

  // Forward power data signals from PowerMonitor (for serial data)
  connect(m_powerMonitor, &PowerMonitor::powerDataReceived, this,
          [this](const PowerData &data) { emit powerDataReceived(data); });
}

void DeviceManager::startBtScanning() {
  m_bluetoothManager->startScanning();
  m_serialManager->disconnect();
}

void DeviceManager::stopBtScanning() {
  m_bluetoothManager->stopScanning();
  // m_serialManager->stopScanning();
}

bool DeviceManager::tryConnect(const QString &portName) {
  //qDebug() << "DeviceManager::tryConnect: Trying to connect to " << portName;
  if (portName.startsWith("ble")) {
    m_bluetoothManager->startScanning();
  } else if (portName.toLower().contains("usb") || portName.startsWith("COM")) {
    QSerialPortInfo serialPortInfo(portName);
    return m_serialManager->connectSerialDevice(serialPortInfo);
  } else {
    //qDebug() << "DeviceManager::tryConnect: Unknown port type: " << portName;
    return false;
  }
  return false;
}
bool DeviceManager::isBLEAutoConnect() const {
  return m_isBluetoothConnected;
}

void DeviceManager::onBluetoothDeviceConnected(const QString &deviceName) {
  m_isBluetoothConnected = true;
  if (this->m_isSerialConnected) {
    m_isSerialConnected = false;
    this->m_serialManager->disconnect();
  }
  this->m_settings->last_device = "ble";
  this->m_settings->saveSettings();
  emit deviceConnected(deviceName + " (Bluetooth)");
}

void DeviceManager::onBluetoothDeviceDisconnected() {
  m_isBluetoothConnected = false;
  if (!m_isSerialConnected) {
    emit deviceDisconnected();
  }
}

void DeviceManager::onSerialDeviceConnected(const QString &deviceName) {
  m_isSerialConnected = true;
  m_isBluetoothConnected = false;
  this->m_settings->last_device = deviceName;
  this->m_settings->saveSettings();
  this->m_bluetoothManager->stopScanning();
  this->m_bluetoothManager->disconnect();
  emit deviceConnected(deviceName + " (Serial)");
}

void DeviceManager::onSerialDeviceDisconnected() {
  m_isSerialConnected = false;
  if (!m_isBluetoothConnected) {
    emit deviceDisconnected();
  }
  dynamic_cast<MainWindow*>(this->parent())->startReconnectTimer();
}

void DeviceManager::onBluetoothDataReceived(const QByteArray &data) {
  // This is now handled directly in BluetoothManager
  // but you can still process raw data here if needed
  // qDebug() << "Raw Bluetooth data received:" << data;
}

void DeviceManager::onSerialDataReceived(PowerData data) {
  emit powerDataReceived(data);
}
