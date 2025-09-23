#include "BluetoothManager.h"

#include "DeviceManager.h"

#include <QDateTime>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QTimer>

// These UUIDs should match your V2-BLE firmware implementation
//#define SERVICE_UUID "01bc9d6f-5b93-41bc-b63f-da5011e34f68"
//#define CHARACTERISTIC_UUID "307fc9ab-5438-4e03-83fa-b9fc3d6afde2"

const QString BluetoothManager::SERVICE_UUID = "{01bc9d6f-5b93-41bc-b63f-da5011e34f68}";
const QString BluetoothManager::DATA_CHARACTERISTIC_UUID = "{307fc9ab-5438-4e03-83fa-b9fc3d6afde2}";

BluetoothManager::BluetoothManager(QObject *parent)
    : QObject(parent)
    , m_discoveryAgent(new QBluetoothDeviceDiscoveryAgent(this))
    , m_controller(nullptr)
    , m_service(nullptr)
    , m_scanTimer(new QTimer(this))
{
    connect(m_discoveryAgent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
            this, &BluetoothManager::onDeviceDiscovered);
    connect(m_discoveryAgent, &QBluetoothDeviceDiscoveryAgent::errorOccurred,
            [this](QBluetoothDeviceDiscoveryAgent::Error error) {
        qDebug() << "Bluetooth discovery error:" << error;
    });
    connect(m_discoveryAgent, &QBluetoothDeviceDiscoveryAgent::finished,
            this, &BluetoothManager::onScanFinished);
    
    // Rescan periodically
    m_scanTimer->setInterval(10000); // 10 seconds
    connect(m_scanTimer, &QTimer::timeout, [this]() {
        if (!m_isConnected && !m_discoveryAgent->isActive()) {
            startScanning();
        }
    });
}

BluetoothManager::~BluetoothManager()
{
    if (m_service) {
        m_service->deleteLater();
    }
    if (m_controller) {
        m_controller->deleteLater();
    }
}

void BluetoothManager::startScanning()
{
    if (m_discoveryAgent->isActive()) {
        return;
    }
    m_isActive = true;
    //qDebug() << "Starting Bluetooth scan...";
    m_discoveryAgent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
    
    if (!m_scanTimer->isActive()) {
        m_scanTimer->start();
    }
}

void BluetoothManager::stopScanning()
{
    //qDebug() << "Stopping Bluetooth scan...";
    m_isActive = false;
    m_scanTimer->stop();
    if (m_discoveryAgent->isActive()) {
        m_discoveryAgent->stop();
    }
}

void BluetoothManager::disconnect()
{
    stopScanning();
    if (m_controller) {
        //qDebug() << "Disconnecting from BLE device...";
        m_controller->disconnectFromDevice();
        m_targetDevice = QBluetoothDeviceInfo();
    }
    m_isConnected = false;
    m_isActive = false;
    //emit deviceDisconnected();
}

void BluetoothManager::onDeviceDiscovered(const QBluetoothDeviceInfo &info)
{
    // Look for devices with "USB Power" or your specific device name
    QString deviceName = info.name();
    // qDebug() << "Found device:" << deviceName << " -";
    if (deviceName.contains("MacWake-USBPowerMeter", Qt::CaseInsensitive)) {
        auto deviceMgr = dynamic_cast<DeviceManager*>(this->parent());
        if (!this->m_isActive) {
            qDebug() << "Ignoring found device - not active";
            return;
        }
        //qDebug() << "Found target device:" << deviceName << info.address();
        m_targetDevice = info;
        m_discoveryAgent->stop();
        connectToDevice(info);
    }
}

void BluetoothManager::onScanFinished()
{
    //qDebug() << "Bluetooth scan finished";
    if (!m_isConnected && m_targetDevice.isValid()) {
        qDebug() << "Connecting to target device " << m_targetDevice.name();
        connectToDevice(m_targetDevice);
    }
}

void BluetoothManager::connectToDevice(const QBluetoothDeviceInfo &device)
{
    if (m_controller) {
        m_controller->deleteLater();
    }
    
    m_controller = QLowEnergyController::createCentral(device, this);
    
    connect(m_controller, &QLowEnergyController::connected,
            this, &BluetoothManager::onControllerConnected);
    connect(m_controller, &QLowEnergyController::disconnected,
            this, &BluetoothManager::onControllerDisconnected);
    connect(m_controller, &QLowEnergyController::serviceDiscovered,
            this, &BluetoothManager::onServiceDiscovered);
    connect(m_controller, &QLowEnergyController::discoveryFinished,
            this, &BluetoothManager::onServiceDiscoveryFinished);
    connect(m_controller, &QLowEnergyController::errorOccurred,
            [this](QLowEnergyController::Error error) {
        qDebug() << "BLE Controller error:" << error;
        m_isConnected = false;
        emit deviceDisconnected();
    });
    
    //qDebug() << "Connecting to device:" << device.name();
    m_controller->connectToDevice();
}

void BluetoothManager::onControllerConnected()
{
    //qDebug() << "BLE Controller connected";
    m_controller->discoverServices();
}

void BluetoothManager::onControllerDisconnected()
{
    //qDebug() << "BLE Controller disconnected";
    m_isConnected = false;
    emit deviceDisconnected();
    
    // Restart scanning
    //QTimer::singleShot(2000, this, &BluetoothManager::startScanning);
}

void BluetoothManager::onServiceDiscovered(const QBluetoothUuid &uuid)
{
    //qDebug() << "Service discovered:" << uuid.toString();
    if (uuid.toString(QUuid::WithBraces) == SERVICE_UUID) {
        //qDebug() << "Found target service";
    }
}

void BluetoothManager::onServiceDiscoveryFinished()
{
    //qDebug() << "Service discovery finished";
    
    QBluetoothUuid serviceUuid(SERVICE_UUID);
    m_service = m_controller->createServiceObject(serviceUuid, this);
    
    if (!m_service) {
        //qDebug() << "Target service not found";
        return;
    }
    
    connect(m_service, &QLowEnergyService::stateChanged,
            this, &BluetoothManager::onServiceStateChanged);
    connect(m_service, &QLowEnergyService::characteristicRead,
            this, &BluetoothManager::onCharacteristicRead);
    connect(m_service, &QLowEnergyService::characteristicChanged,
            this, &BluetoothManager::onCharacteristicChanged);
    
    m_service->discoverDetails();
}

void BluetoothManager::onServiceStateChanged(QLowEnergyService::ServiceState state)
{
    if (state == QLowEnergyService::RemoteServiceDiscovered) {
        setupService();
    }
}

void BluetoothManager::setupService()
{
    if (!m_service) return;
    
    // Find the data characteristic
    QBluetoothUuid characteristicUuid(DATA_CHARACTERISTIC_UUID);
    m_dataCharacteristic = m_service->characteristic(characteristicUuid);
    
    if (!m_dataCharacteristic.isValid()) {
        //qDebug() << "Data characteristic not found";
        return;
    }
    
    //qDebug() << "Found characteristic with properties:" << m_dataCharacteristic.properties();
    
    // Enable notifications if supported
    if (m_dataCharacteristic.properties() & QLowEnergyCharacteristic::Notify) {
        //qDebug() << "Enabling notifications...";
        
        // Find the Client Characteristic Configuration Descriptor (CCCD)
        QLowEnergyDescriptor cccd = m_dataCharacteristic.descriptor(
            QBluetoothUuid(static_cast<quint16>(0x2902))); // Standard CCCD UUID
            
        if (cccd.isValid()) {
            // Enable notifications by writing 0x0100 to CCCD
            m_service->writeDescriptor(cccd, QByteArray::fromHex("0100"));
            //qDebug() << "Notification enabled via CCCD";
        } else {
            //qDebug() << "CCCD not found - notifications may not work";
        }
    }
    
    // Check if indications are supported as fallback
    else if (m_dataCharacteristic.properties() & QLowEnergyCharacteristic::Indicate) {
        //qDebug() << "Enabling indications...";
        
        QLowEnergyDescriptor cccd = m_dataCharacteristic.descriptor(
            QBluetoothUuid(static_cast<quint16>(0x2902)));
            
        if (cccd.isValid()) {
            // Enable indications by writing 0x0200 to CCCD
            m_service->writeDescriptor(cccd, QByteArray::fromHex("0200"));
            //qDebug() << "Indications enabled via CCCD";
        }
    }
    else {
        //qDebug() << "Characteristic doesn't support notifications or indications";
    }
    
    m_isConnected = true;
    emit deviceConnected(m_targetDevice.name());
    
    //qDebug() << "BLE service setup complete";
}

void BluetoothManager::onCharacteristicRead(const QLowEnergyCharacteristic &characteristic, const QByteArray &value)
{
    if (characteristic == m_dataCharacteristic) {
        emit dataReceived(value);
    }
}

void BluetoothManager::onCharacteristicChanged(const QLowEnergyCharacteristic &characteristic, const QByteArray &value)
{
    if (characteristic == m_dataCharacteristic) {
        //qDebug() << "Received notification data:" << value;
        emit dataReceived(value);  // Keep raw data signal for compatibility
        parseJsonAndEmitPowerData(value);  // Parse and emit PowerData
    }
}

void BluetoothManager::parseJsonAndEmitPowerData(const QByteArray &data)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "JSON parse error:" << error.errorString();
        qWarning() << "Raw data:" << data;
        return;
    }
    
    if (!doc.isObject()) {
        qWarning() << "JSON data is not an object";
        return;
    }
    
    QJsonObject json = doc.object();
    PowerData powerData = parseJsonToPowerData(json);
    
    // Update energy accumulation for BLE data
    if (m_lastTimestamp != 0) {
        double timeDelta = (powerData.timestamp - m_lastTimestamp) / 1000.0; // Convert to seconds
        if (timeDelta > 0 && timeDelta < 3600) { // Sanity check (less than 1 hour)
            m_energyAccumulator += powerData.power * (timeDelta / 3600.0); // Convert to hours
        }
    }
    powerData.energy = m_energyAccumulator;
    m_lastTimestamp = powerData.timestamp;
    
    // qDebug() << "Parsed BLE data - V:" << powerData.voltage << "A:" << powerData.current
    //          << "W:" << powerData.power << "E:" << powerData.energy;
    
    emit powerDataReceived(powerData);
}

PowerData BluetoothManager::parseJsonToPowerData(const QJsonObject &json)
{
    PowerData data;
    
    // Parse JSON fields from your device format
    data.current = json.value("current").toDouble(0.0);
    data.voltage = json.value("voltage").toDouble(0.0);
    data.power = json.value("power").toDouble(0.0);
    data.energy = json.value("charge").toDouble(0.0);
    // You might need to convert charge to energy units here

    // Use device timestamp if available, otherwise use current time
    data.timestamp = json.value("timestamp").toVariant().toULongLong();
    if (data.timestamp == 0) {
        data.timestamp = QDateTime::currentMSecsSinceEpoch();
    }
    
    return data;
}
