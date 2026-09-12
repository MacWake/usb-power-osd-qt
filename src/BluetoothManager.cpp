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

#include <QBluetoothLocalDevice>
#include <QBluetoothAddress>

const QString BluetoothManager::SERVICE_UUID = "{01bc9d6f-5b93-41bc-b63f-da5011e34f68}";
const QString BluetoothManager::DATA_CHARACTERISTIC_UUID = "{307fc9ab-5438-4e03-83fa-b9fc3d6afde2}";

BluetoothManager::BluetoothManager(QObject *parent)
    : PowerDataSource(parent)
    , m_discoveryAgent(new QBluetoothDeviceDiscoveryAgent(this))
    , m_controller(nullptr)
    , m_service(nullptr)
    , m_scanTimer(new QTimer(this))
    , m_connectTimer(new QTimer(this))
    , m_cleanupTimer(new QTimer(this))
{
    m_cleanupTimer->setSingleShot(true);
    connect(m_cleanupTimer, &QTimer::timeout, this, &BluetoothManager::cleanupController);

    connect(this, &BluetoothManager::powerDataReceived, this,
            [this](const PowerData &data) { emit sampleReceived(data); });
    this->m_isActive = false;
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
    connect(m_scanTimer, &QTimer::timeout, [this] {
        if (!m_isConnected && !m_isConnecting && !m_discoveryAgent->isActive()) {
            startScanning();
        }
    });

    // Connection timeout: covers controller connection, service discovery, and
    // characteristic subscription. If any phase stalls, clean up and try again.
    m_connectTimer->setSingleShot(true);
    m_connectTimer->setInterval(15000); // 15 seconds
    connect(m_connectTimer, &QTimer::timeout, [this] {
        qDebug() << "BLE connection/subscription timed out, aborting";

        bool wasConnecting = false;
        {
            QMutexLocker lock(&m_stateMutex);
            wasConnecting = m_isConnecting;
            m_isConnecting = false;
            m_isConnected = false;
            m_controllerConnected = false;
        }
        cleanupControllerAsync();
        if (wasConnecting) {
            emit disconnected();
        }
        // Delay rescan so BlueZ/CoreBluetooth can finish processing the disconnect/cancel
        QTimer::singleShot(3000, this, &BluetoothManager::startScanning);
    });

    // Log local adapter info
    QList<QBluetoothHostInfo> adapters = QBluetoothLocalDevice::allDevices();
    qDebug() << "Found" << adapters.count() << "local Bluetooth adapters:";
    for (const QBluetoothHostInfo &adapter : adapters) {
        QBluetoothLocalDevice localDevice(adapter.address());
        if (localDevice.isValid()) {
            // Check for BLE support (Qt 5.7+)
            // QBluetoothDeviceDiscoveryAgent::LowEnergyMethod is used in startScanning
            // but we can check if the adapter itself is powered on and its address
            qDebug() << "  Adapter:" << adapter.name() << "[" << adapter.address().toString() << "]";
            qDebug() << "    Connected:" << (localDevice.hostMode() != QBluetoothLocalDevice::HostPoweredOff);
        }
    }
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
    {
        QMutexLocker lock(&m_stateMutex);
        if (m_discoveryAgent->isActive() || m_isConnected || m_isConnecting) {
            return;
        }
        m_isActive = true;
    }

    auto supportedDiscoveryMethods = QBluetoothDeviceDiscoveryAgent::supportedDiscoveryMethods();
    qDebug() << "Supported discovery methods:" << supportedDiscoveryMethods;
    if (!(supportedDiscoveryMethods & QBluetoothDeviceDiscoveryAgent::LowEnergyMethod)) {
        qWarning() << "CRITICAL: This Bluetooth adapter DOES NOT support Low Energy (BLE) discovery!";
    }

    qDebug() << "Starting Bluetooth scan (All methods)...";
    emit discoveryStatusChanged(tr("Scanning for Bluetooth devices..."));
    m_discoveryAgent->setLowEnergyDiscoveryTimeout(10000);
    m_discoveryAgent->start();

    if (!m_scanTimer->isActive()) {
        m_scanTimer->start();
    }
}

void BluetoothManager::stopScanning()
{
    qDebug() << "Stopping Bluetooth scan...";
    {
        QMutexLocker lock(&m_stateMutex);
        m_isActive = false;
    }
    m_scanTimer->stop();
    if (m_discoveryAgent->isActive()) {
        m_discoveryAgent->stop();
    }
    emit discoveryStatusChanged(QString{});
}

void BluetoothManager::disconnect()
{
    stopScanning();
    {
        QMutexLocker lock(&m_stateMutex);
        if (m_controller) {
            qDebug() << "Disconnecting from BLE device...";
            // Avoid blocking the main thread; the actual cleanup is deferred.
            cleanupControllerAsync();
        }
        m_targetDevice = QBluetoothDeviceInfo();
        m_isConnected = false;
        m_isActive = false;
    }
    emit discoveryStatusChanged(QString{});
}

void BluetoothManager::onDeviceDiscovered(const QBluetoothDeviceInfo &info)
{
    // Look for devices with "USB Power" or your specific device name
    const QString deviceName = info.name();
    const QList<QBluetoothUuid> serviceUuids = info.serviceUuids();

    const QString address = info.address().toString();
    qDebug() << "Found device:" << (deviceName.isEmpty() ? "<no name>" : deviceName)
             << "[" << address << "]"
             << "RSSI:" << info.rssi();

    // Check if it matches our target name
    bool isTarget = deviceName.contains("MacWake-USBPowerMeter", Qt::CaseInsensitive) ||
                    deviceName.contains("MacWake PowerMeter", Qt::CaseInsensitive) ||
                    deviceName.contains("USB Power", Qt::CaseInsensitive) ||
                    deviceName.contains("Power Meter", Qt::CaseInsensitive) ||
                    deviceName.contains("PowerMeter", Qt::CaseInsensitive) ||
                    deviceName.contains("USB-Power", Qt::CaseInsensitive) ||
                    deviceName.contains("USBPower", Qt::CaseInsensitive);

    // Also check if it advertises our target service UUID
    if (!isTarget) {
        for (const auto& uuid : serviceUuids) {
            QString uuidStr = uuid.toString(QUuid::WithBraces);
            if (uuidStr.compare(SERVICE_UUID, Qt::CaseInsensitive) == 0 ||
                uuid.toString(QUuid::WithoutBraces).compare(SERVICE_UUID.mid(1, SERVICE_UUID.length() - 2), Qt::CaseInsensitive) == 0) {
                isTarget = true;
                break;
            }
        }
    }

    if (!isTarget) {
        return;
    }

    {
        QMutexLocker lock(&m_stateMutex);
        if (!this->m_isActive) {
            qDebug() << "Ignoring found target device - not active";
            return;
        }
    }

    QMutexLocker lock(&m_stateMutex);

    if (m_isConnecting || m_isConnected) {
        qDebug() << "Ignoring found target device - already connecting/connected";
        return;
    }

    const QString displayName = deviceName.isEmpty() ? tr("<no name>") : deviceName;
    qDebug() << "Target device identified:" << displayName
             << "[" << address << "]";
    emit discoveryStatusChanged(
        tr("Found: %1 [%2]").arg(displayName, address));
    m_targetDevice = info;

    // Mark connecting early so any synchronous finished() signal emitted by
    // stop() does not re-enter connectToDevice() while we are still in this path.
    m_isConnecting = true;
    lock.unlock();

    // Stop discovery before opening a connection. Leaving the agent running on
    // some platforms (macOS) can cause the controller to hang while trying to
    // resolve the device at the same time the discovery agent owns the adapter.
    // Do this outside the mutex: stop() may emit finished() synchronously.
    if (m_discoveryAgent->isActive()) {
        m_discoveryAgent->stop();
    }

    connectToDevice(info);
}

void BluetoothManager::onScanFinished()
{
    qDebug() << "Bluetooth scan finished";
    if (!m_isConnected && !m_controller && m_targetDevice.isValid()) {
        qDebug() << "Connecting to target device " << m_targetDevice.name();
        emit discoveryStatusChanged(tr("Connecting to %1...")
                                         .arg(m_targetDevice.name().isEmpty()
                                                  ? tr("<no name>")
                                                  : m_targetDevice.name()));
        connectToDevice(m_targetDevice);
    } else if (!m_isConnected && !m_isConnecting) {
        emit discoveryStatusChanged(tr("No Bluetooth device found"));
    }
}

void BluetoothManager::cleanupController()
{
    m_connectTimer->stop();
    m_cleanupTimer->stop();
    {
        QMutexLocker lock(&m_stateMutex);
        m_isConnecting = false;
        m_isConnected = false;
        m_controllerConnected = false;
    }
    if (m_controller) {
        m_controller->disconnect(this);
        // On macOS, disconnectFromDevice() can block the main thread if the
        // peripheral disappeared uncleanly. Delete the controller directly and
        // let Qt/CoreBluetooth clean up asynchronously.
        auto *oldController = m_controller;
        m_controller = nullptr;
        oldController->deleteLater();
    }
    if (m_service) {
        m_service->disconnect(this);
        auto *oldService = m_service;
        m_service = nullptr;
        oldService->deleteLater();
    }
}

void BluetoothManager::cleanupControllerAsync()
{
    if (!m_controller && !m_service) {
        return;
    }
    // Defer the actual cleanup so any in-flight signals are delivered before
    // the controller is destroyed. A short timeout guarantees we don't wait
    // forever if CoreBluetooth is stuck.
    m_cleanupTimer->start(250);
}

void BluetoothManager::connectToDevice(const QBluetoothDeviceInfo &device)
{
    // Cleanup must run without holding m_stateMutex because cleanupController()
    // also locks m_stateMutex; otherwise we deadlock on this thread.
    cleanupController();

    {
        QMutexLocker lock(&m_stateMutex);
        m_isConnecting = true;
        m_isConnected = false;
        m_controllerConnected = false;
        m_retryCount = 0;
    }

    const QString displayName = device.name().isEmpty() ? tr("<no name>") : device.name();
    emit discoveryStatusChanged(tr("Connecting to %1...").arg(displayName));

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
        qDebug() << "BLE Controller error:" << error << "-" << m_controller->errorString();
        
        bool transientError = (error == QLowEnergyController::ConnectionError ||
                               error == QLowEnergyController::UnknownError);

        if (m_isConnecting && transientError && m_retryCount < m_maxRetries) {
            m_retryCount++;
            int delay = 1000 * m_retryCount; // Exponential-ish backoff
            qDebug() << "Attempting retry" << m_retryCount << "of" << m_maxRetries << "in" << delay << "ms";

            {
                QMutexLocker lock(&m_stateMutex);
                m_isConnecting = false;
                m_controllerConnected = false;
            }
            m_connectTimer->stop();

            QTimer::singleShot(delay, [this]() {
                QMutexLocker lock(&m_stateMutex);
                if (m_targetDevice.isValid() && !m_isConnected && !m_isConnecting) {
                    lock.unlock();
                    connectToDevice(m_targetDevice);
                }
            });
            return;
        }

        {
            QMutexLocker lock(&m_stateMutex);
            m_isConnected = false;
            m_isConnecting = false;
            m_controllerConnected = false;
            m_retryCount = 0;
        }
        cleanupControllerAsync();
        emit disconnected();
    });

    m_connectTimer->start();
    qDebug() << "Connecting to device:" << device.name();
    m_controller->connectToDevice();
}

void BluetoothManager::start() { startScanning(); }
void BluetoothManager::stop() { disconnect(); }

QString BluetoothManager::sourceName() const {
    return m_targetDevice.name();
}

bool BluetoothManager::isConnected() const {
    return m_isConnected;
}

void BluetoothManager::onControllerConnected()
{
    qDebug() << "BLE Controller connected";
    // Keep m_isConnecting true until service discovery finishes and the data
    // characteristic is subscribed. This prevents startScanning()/reconnect logic
    // from interrupting the connection mid-setup.
    {
        QMutexLocker lock(&m_stateMutex);
        m_controllerConnected = true;
        m_retryCount = 0;
    }
    // Restart the timeout to cover the remaining service-discovery/subscription phase.
    m_connectTimer->start();
    m_controller->discoverServices();
}

void BluetoothManager::onControllerDisconnected()
{
    qDebug() << "BLE Controller disconnected. RSSI:" << m_targetDevice.rssi();
    {
        QMutexLocker lock(&m_stateMutex);
        m_isConnected = false;
        m_isConnecting = false;
        m_controllerConnected = false;
        m_retryCount = 0;
    }
    cleanupControllerAsync();
    emit disconnected();

    // If it was an active connection that dropped, try to reconnect or scan
    {
        QMutexLocker lock(&m_stateMutex);
        if (m_isActive) {
            qDebug() << "Unexpected disconnect while active, starting scan to reconnect...";
            QTimer::singleShot(2000, this, &BluetoothManager::startScanning);
        }
    }
}

void BluetoothManager::onServiceDiscovered(const QBluetoothUuid &uuid)
{
    qDebug() << "Service discovered:" << uuid.toString();
    QString uuidWithBraces = uuid.toString(QUuid::WithBraces);
    if (uuidWithBraces.compare(SERVICE_UUID, Qt::CaseInsensitive) == 0) {
        qDebug() << "Found target service";
    }
}

void BluetoothManager::onServiceDiscoveryFinished()
{
    qDebug() << "Service discovery finished";

    // The controller is connected and services are discovered; keep the timeout
    // running to cover the final service-detail discovery + descriptor write.
    m_connectTimer->start();

    QBluetoothUuid serviceUuid(SERVICE_UUID);
    m_service = m_controller->createServiceObject(serviceUuid, this);
    
    if (!m_service) {
        // Try without braces if first attempt failed
        QString cleanUuid = SERVICE_UUID;
        if (cleanUuid.startsWith('{') && cleanUuid.endsWith('}')) {
            cleanUuid = cleanUuid.mid(1, cleanUuid.length() - 2);
        }
        m_service = m_controller->createServiceObject(QBluetoothUuid(cleanUuid), this);
    }

    if (!m_service) {
        qDebug() << "Target service not found";
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
        qDebug() << "Data characteristic not found";
        return;
    }
    
    qDebug() << "Found characteristic with properties:" << m_dataCharacteristic.properties();
    
    // Enable notifications if supported
    if (m_dataCharacteristic.properties() & QLowEnergyCharacteristic::Notify) {
        qDebug() << "Enabling notifications...";
        
        // Find the Client Characteristic Configuration Descriptor (CCCD)
        QLowEnergyDescriptor cccd = m_dataCharacteristic.descriptor(
            QBluetoothUuid(static_cast<quint16>(0x2902))); // Standard CCCD UUID
            
        if (cccd.isValid()) {
            // Enable notifications by writing 0x0100 to CCCD
            m_service->writeDescriptor(cccd, QByteArray::fromHex("0100"));
            qDebug() << "Notification enabled via CCCD";
        } else {
            qDebug() << "CCCD not found - notifications may not work";
        }
    }
    
    // Check if indications are supported as fallback
    else if (m_dataCharacteristic.properties() & QLowEnergyCharacteristic::Indicate) {
        qDebug() << "Enabling indications...";
        
        QLowEnergyDescriptor cccd = m_dataCharacteristic.descriptor(
            QBluetoothUuid(static_cast<quint16>(0x2902)));
            
        if (cccd.isValid()) {
            // Enable indications by writing 0x0200 to CCCD
            m_service->writeDescriptor(cccd, QByteArray::fromHex("0200"));
            qDebug() << "Indications enabled via CCCD";
        }
    }
    else {
        qDebug() << "Characteristic doesn't support notifications or indications";
    }

    // Subscription is in flight; stop the guard timer once the OS confirms the
    // descriptor write landed. If it never lands, the timeout will clean up.
    m_connectTimer->stop();

    {
        QMutexLocker lock(&m_stateMutex);
        m_isConnected = true;
        m_isConnecting = false;
        m_controllerConnected = true;
    }
    emit connected(m_targetDevice.name());

    qDebug() << "BLE service setup complete";
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
