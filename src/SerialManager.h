#ifndef SERIALMANAGER_H
#define SERIALMANAGER_H

#include "PowerData.h"
#include "PowerDataSource.h"
#include "SerialAdapter.h"

#include <QSerialPort>
#include <QSerialPortInfo>

#include <memory>
#include <vector>

class SerialManager : public PowerDataSource {
  Q_OBJECT

public:
  explicit SerialManager(QObject *parent = nullptr);
  ~SerialManager() override;

  void start() override;
  void stop() override;
  [[nodiscard]] QString sourceName() const override;
  [[nodiscard]] bool isConnected() const override;

  Q_INVOKABLE bool connectSerialDevice(const QSerialPortInfo &portInfo);
  Q_INVOKABLE void disconnect();

public slots:
  bool tryConnect(const QString &portName);

private slots:
  void onSerialDataReady();
  void onSerialError(QSerialPort::SerialPortError error);

private:
  [[nodiscard]] bool tryAdapters();

  QSerialPort *m_serialPort;
  bool m_isConnected = false;

  std::vector<std::unique_ptr<SerialAdapter>> m_adapters;
  SerialAdapter *m_currentAdapter = nullptr;
};

#endif // SERIALMANAGER_H
