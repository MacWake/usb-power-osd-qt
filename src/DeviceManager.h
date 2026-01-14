#ifndef DEVICEMANAGER_H
#define DEVICEMANAGER_H

#include "BluetoothManager.h"
#include "OsdSettings.h"
#include "PowerMonitor.h"
#include "SerialManager.h"
#include <QObject>
#include <QThread>

class DeviceManager : public QObject {
  Q_OBJECT

public:
  explicit DeviceManager(QObject *parent = nullptr);
  ~DeviceManager() override;

  void startBtScanning();
  void stopBtScanning();
  bool tryConnect(const QString &portName);
  void setSettings(OsdSettings *settings) { m_settings = settings; }
  bool isBLEAutoConnect() const;

signals:
  void deviceConnected(const QString &deviceName);
  void deviceDisconnected();
  void powerDataReceived(const PowerData &powerData); // Add this signal

private slots:
  void onBluetoothDeviceConnected(const QString &deviceName);
  void onBluetoothDeviceDisconnected();
  void onSerialDeviceConnected(const QString &deviceName);
  void onSerialDeviceDisconnected();
  void onSerialDataReceived(const PowerData &data);

private:
  BluetoothManager *m_bluetoothManager;
  SerialManager *m_serialManager;
  QThread *m_serialThread;
  PowerMonitor *m_powerMonitor;
  OsdSettings *m_settings = nullptr;

  bool m_isBluetoothConnected = false;
  bool m_isSerialConnected = false;
};

#endif // DEVICEMANAGER_H
