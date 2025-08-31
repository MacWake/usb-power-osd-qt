#include "DeviceManager.h"
#include "PowerMonitor.h"
#include <QDebug>

DeviceManager::DeviceManager(QObject *parent)
    : QObject(parent)
    , m_bluetoothManager(new BluetoothManager(this))
    , m_serialManager(new SerialManager(this))
    , m_powerMonitor(new PowerMonitor(this))
{
    // Connect Bluetooth signals
    connect(m_bluetoothManager, &BluetoothManager::deviceConnected,
            this, &DeviceManager::onBluetoothDeviceConnected);
    connect(m_bluetoothManager, &BluetoothManager::deviceDisconnected,
            this, &DeviceManager::onBluetoothDeviceDisconnected);
    // connect(m_bluetoothManager, &BluetoothManager::dataReceived,
    //         this, &DeviceManager::onBluetoothDataReceived);
    
    // NEW: Connect to parsed power data from Bluetooth
    connect(m_bluetoothManager, &BluetoothManager::powerDataReceived,
            this, [this](const PowerData &data) {
        // Forward the parsed power data directly
        emit powerDataReceived(data);
    });
    
    // Connect Serial signals
    connect(m_serialManager, &SerialManager::deviceConnected,
            this, &DeviceManager::onSerialDeviceConnected);
    connect(m_serialManager, &SerialManager::deviceDisconnected,
            this, &DeviceManager::onSerialDeviceDisconnected);
    connect(m_serialManager, &SerialManager::dataReceived,
            this, &DeviceManager::onSerialDataReceived);
    
    // Forward power data signals from PowerMonitor (for serial data)
    connect(m_powerMonitor, &PowerMonitor::powerDataReceived,
            this, [this](const PowerData &data) {
        emit powerDataReceived(data);
    });
}

// ADD THESE MISSING METHODS:

void DeviceManager::startScanning()
{
    m_bluetoothManager->startScanning();
    m_serialManager->startScanning();
}

void DeviceManager::stopScanning()
{
    m_bluetoothManager->stopScanning();
    m_serialManager->stopScanning();
}

void DeviceManager::requestPowerData()
{
    if (m_isBluetoothConnected) {
        m_bluetoothManager->requestData();
    } else if (m_isSerialConnected) {
        m_serialManager->requestData();
    }
}

void DeviceManager::onBluetoothDeviceConnected(const QString &deviceName)
{
    m_isBluetoothConnected = true;
    emit deviceConnected(deviceName + " (Bluetooth)");
}

void DeviceManager::onBluetoothDeviceDisconnected()
{
    m_isBluetoothConnected = false;
    if (!m_isSerialConnected) {
        emit deviceDisconnected();
    }
}

void DeviceManager::onSerialDeviceConnected(const QString &deviceName)
{
    m_isSerialConnected = true;
    emit deviceConnected(deviceName + " (Serial)");
}

void DeviceManager::onSerialDeviceDisconnected()
{
    m_isSerialConnected = false;
    if (!m_isBluetoothConnected) {
        emit deviceDisconnected();
    }
}

void DeviceManager::onBluetoothDataReceived(const QByteArray &data)
{
    // This is now handled directly in BluetoothManager
    // but you can still process raw data here if needed
    // qDebug() << "Raw Bluetooth data received:" << data;
}

void DeviceManager::onSerialDataReceived(const QByteArray &data)
{
    // Serial data still goes through PowerMonitor for parsing
    m_powerMonitor->processSerialData(data);
}
