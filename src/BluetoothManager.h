#ifndef BLUETOOTHMANAGER_H
#define BLUETOOTHMANAGER_H

#include "PowerDataSource.h"
#include "PowerMonitor.h"

#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothDeviceInfo>
#include <QLowEnergyController>
#include <QLowEnergyService>
#include <QJsonDocument>
#include <QJsonObject>

QT_FORWARD_DECLARE_CLASS(QTimer)

class BluetoothManager : public PowerDataSource
{
    Q_OBJECT

public:
    explicit BluetoothManager(QObject *parent = nullptr);
    ~BluetoothManager() override;

    void start() override;
    void stop() override;
    [[nodiscard]] QString sourceName() const override;
    [[nodiscard]] bool isConnected() const override;

    void startScanning();
    void stopScanning();
    void disconnect();

    /**
     * @brief Parse a V2-BLE JSON payload into a PowerData sample.
     *
     * Exposed as a public static helper so it can be unit-tested without
     * instantiating a BluetoothManager or connecting to hardware.
     */
    static PowerData parseJsonToPowerData(const QJsonObject &json);

  signals:
    void dataReceived(const QByteArray &data);           // Keep raw data signal
    void powerDataReceived(const PowerData &powerData); // Parsed data signal

private slots:
    void onDeviceDiscovered(const QBluetoothDeviceInfo &info);
    void onScanFinished();
    void onControllerConnected();
    void onControllerDisconnected();
    void onServiceDiscovered(const QBluetoothUuid &uuid);
    void onServiceDiscoveryFinished();
    void onServiceStateChanged(QLowEnergyService::ServiceState state);
    void onCharacteristicRead(const QLowEnergyCharacteristic &characteristic, const QByteArray &value);
    void onCharacteristicChanged(const QLowEnergyCharacteristic &characteristic, const QByteArray &value);

private:
    void connectToDevice(const QBluetoothDeviceInfo &device);
    void cleanupController();
    void cleanupControllerAsync();
    void setupService();
    void parseJsonAndEmitPowerData(const QByteArray &data);

    QBluetoothDeviceDiscoveryAgent *m_discoveryAgent;
    QLowEnergyController *m_controller;
    QLowEnergyService *m_service;

    QBluetoothDeviceInfo m_targetDevice;
    QLowEnergyCharacteristic m_dataCharacteristic;

    QTimer *m_scanTimer;
    QTimer *m_connectTimer;
    QTimer *m_cleanupTimer;
    bool m_isConnected = false;
    bool m_isConnecting = false;
    int m_retryCount = 0;
    const int m_maxRetries = 3;
    
    // Energy accumulation for BLE data
    double m_energyAccumulator = 0.0;
    quint64 m_lastTimestamp = 0;
    bool m_isActive;

    // USB Power OSD V2-BLE service and characteristic UUIDs
    static const QString SERVICE_UUID;
    static const QString DATA_CHARACTERISTIC_UUID;
};

#endif // BLUETOOTHMANAGER_H
