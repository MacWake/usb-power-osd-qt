#include "DeviceManager.h"

#include "MainWindow.h"
#include "PowerMonitor.h"
#include <QDebug>

DeviceManager::DeviceManager(QObject *parent)
    : QObject(parent), m_bluetoothManager(new BluetoothManager(this)),
      m_serialManager(new SerialManager()),
      m_serialThread(new QThread(this)),
      m_powerMonitor(new PowerMonitor(this)) {
  m_serialManager->moveToThread(m_serialThread);

  // Connect common source signals for both backends
  connect(m_bluetoothManager, &PowerDataSource::connected, this,
          &DeviceManager::onSourceConnected);
  connect(m_bluetoothManager, &PowerDataSource::disconnected, this,
          &DeviceManager::onSourceDisconnected);
  connect(m_bluetoothManager, &PowerDataSource::sampleReceived, this,
          &DeviceManager::onSampleReceived);

  connect(m_serialManager, &PowerDataSource::connected, this,
          &DeviceManager::onSourceConnected);
  connect(m_serialManager, &PowerDataSource::disconnected, this,
          &DeviceManager::onSourceDisconnected);
  connect(m_serialManager, &PowerDataSource::sampleReceived, this,
          &DeviceManager::onSampleReceived);

  m_serialThread->start();
}

DeviceManager::~DeviceManager() {
  m_serialThread->quit();
  m_serialThread->wait();
  delete m_serialManager;
}

void DeviceManager::setActiveSource(PowerDataSource *source) {
  if (m_activeSource && m_activeSource != source) {
    m_activeSource->stop();
  }
  m_activeSource = source;
}

void DeviceManager::startBtScanning() {
  setActiveSource(m_bluetoothManager);
  m_bluetoothManager->startScanning();
  QMetaObject::invokeMethod(m_serialManager, "disconnect", Qt::QueuedConnection);
}

void DeviceManager::stopBtScanning() {
  m_bluetoothManager->stopScanning();
  m_bluetoothManager->disconnect();
}

bool DeviceManager::tryConnect(const QString &portName) {
  if (portName.startsWith("ble")) {
    setActiveSource(m_bluetoothManager);
    m_bluetoothManager->startScanning();
    return true;
  }

  setActiveSource(m_serialManager);
  bool result = false;
  if (QMetaObject::invokeMethod(m_serialManager, "tryConnect",
                                Qt::BlockingQueuedConnection,
                                Q_RETURN_ARG(bool, result),
                                Q_ARG(QString, portName))) {
    return result;
  }

  return false;
}

bool DeviceManager::isBLEAutoConnect() const {
  return m_isBluetoothConnected;
}

void DeviceManager::onSourceConnected(const QString &deviceName) {
  if (auto *source = qobject_cast<PowerDataSource *>(sender())) {
    if (source == m_bluetoothManager) {
      m_isBluetoothConnected = true;
      m_isSerialConnected = false;
      QMetaObject::invokeMethod(m_serialManager, "disconnect", Qt::QueuedConnection);
    } else if (source == m_serialManager) {
      m_isSerialConnected = true;
      m_isBluetoothConnected = false;
      m_bluetoothManager->stopScanning();
      m_bluetoothManager->disconnect();
    }

    if (m_settings) {
      m_settings->last_device = deviceName;
      m_settings->saveSettings();
    }

    QString suffix;
    if (source == m_bluetoothManager) {
      suffix = " (Bluetooth)";
    } else if (source == m_serialManager) {
      suffix = " (Serial)";
    }
    emit deviceConnected(deviceName + suffix);
  }
}

void DeviceManager::onSourceDisconnected() {
  if (auto *source = qobject_cast<PowerDataSource *>(sender())) {
    if (source == m_bluetoothManager) {
      m_isBluetoothConnected = false;
    } else if (source == m_serialManager) {
      m_isSerialConnected = false;
    }

    if (!m_isBluetoothConnected && !m_isSerialConnected) {
      emit deviceDisconnected();
      if (auto *mw = qobject_cast<MainWindow *>(parent())) {
        mw->startReconnectTimer();
      }
    }
  }
}

void DeviceManager::onSampleReceived(const PowerData &data) {
  if (auto *source = qobject_cast<PowerDataSource *>(sender())) {
    if (source == m_serialManager) {
      // Serial data is already in PowerData form; forward directly.
      emit powerDataReceived(data);
      return;
    }
    if (source == m_bluetoothManager) {
      // BLE JSON path already provides parsed PowerData; the PowerMonitor is
      // kept around for future binary BLE/serial parsing.
      emit powerDataReceived(data);
      return;
    }
  }
  emit powerDataReceived(data);
}
