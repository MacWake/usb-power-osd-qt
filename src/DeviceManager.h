#ifndef DEVICEMANAGER_H
#define DEVICEMANAGER_H

#include "BluetoothManager.h"
#include "SerialManager.h"
#include "PowerMonitor.h"
#include <QObject>

class DeviceManager : public QObject
{
    Q_OBJECT

public:
    explicit DeviceManager(QObject *parent = nullptr);
    
    void startScanning();
    void stopScanning();
    void requestPowerData();

signals:
    void deviceConnected(const QString &deviceName);
    void deviceDisconnected();
    void powerDataReceived(const PowerData &powerData);  // Add this signal

private slots:
    void onBluetoothDeviceConnected(const QString &deviceName);
    void onBluetoothDeviceDisconnected();
    void onBluetoothDataReceived(const QByteArray &data);
    void onSerialDeviceConnected(const QString &deviceName);
    void onSerialDeviceDisconnected();
    void onSerialDataReceived(const QByteArray &data);

private:
    BluetoothManager *m_bluetoothManager;
    SerialManager *m_serialManager;
    PowerMonitor *m_powerMonitor;
    
    bool m_isBluetoothConnected = false;
    bool m_isSerialConnected = false;
};

#endif // DEVICEMANAGER_H
