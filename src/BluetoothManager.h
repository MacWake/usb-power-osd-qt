#ifndef BLUETOOTHMANAGER_H
#define BLUETOOTHMANAGER_H

#include <QObject>
#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothDeviceInfo>
#include <QLowEnergyController>
#include <QLowEnergyService>
#include <QJsonDocument>
#include <QJsonObject>
#include "PowerMonitor.h"

QT_FORWARD_DECLARE_CLASS(QTimer)

class BluetoothManager : public QObject
{
    Q_OBJECT

public:
    explicit BluetoothManager(QObject *parent = nullptr);
    ~BluetoothManager() override;
    
    void startScanning();
    void stopScanning();
    void requestData();

signals:
    void deviceConnected(const QString &deviceName);
    void deviceDisconnected();
    void dataReceived(const QByteArray &data);           // Keep raw data signal
    void powerDataReceived(const PowerData &powerData); // Add parsed data signal

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
    void setupService();
    void parseJsonAndEmitPowerData(const QByteArray &data);
    static PowerData parseJsonToPowerData(const QJsonObject &json);
    
    QBluetoothDeviceDiscoveryAgent *m_discoveryAgent;
    QLowEnergyController *m_controller;
    QLowEnergyService *m_service;
    
    QBluetoothDeviceInfo m_targetDevice;
    QLowEnergyCharacteristic m_dataCharacteristic;
    
    QTimer *m_scanTimer;
    bool m_isConnected = false;
    
    // Energy accumulation for BLE data
    double m_energyAccumulator = 0.0;
    quint64 m_lastTimestamp = 0;
    
    // USB Power OSD V2-BLE service and characteristic UUIDs
    static const QString SERVICE_UUID;
    static const QString DATA_CHARACTERISTIC_UUID;
};

#endif // BLUETOOTHMANAGER_H
