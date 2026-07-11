#ifndef DEVICEMANAGER_H
#define DEVICEMANAGER_H

#include "OsdSettings.h"
#include "PowerData.h"
#include "PowerDataSource.h"
#include "PowerMonitor.h"
#include "SerialManager.h"
#include "BluetoothManager.h"
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

  void setActiveSource(PowerDataSource *source);
  [[nodiscard]] PowerDataSource *activeSource() const { return m_activeSource; }

signals:
  void deviceConnected(const QString &deviceName);
  void deviceDisconnected();
  void powerDataReceived(const PowerData &powerData);

private slots:
  void onSourceConnected(const QString &deviceName);
  void onSourceDisconnected();
  void onSampleReceived(const PowerData &data);

private:
  BluetoothManager *m_bluetoothManager;
  SerialManager *m_serialManager;
  QThread *m_serialThread;
  PowerMonitor *m_powerMonitor;
  OsdSettings *m_settings = nullptr;

  PowerDataSource *m_activeSource = nullptr;

  bool m_isBluetoothConnected = false;
  bool m_isSerialConnected = false;
};

#endif // DEVICEMANAGER_H
