// PowerDataSource.h
#pragma once

#include "PowerData.h"
#include <QObject>
#include <QString>

/**
 * @brief Abstract base class for all power measurement backends.
 *
 * Implementations (Serial, BLE, TCP/IP, ...) run on their own event path and
 * emit sampleReceived() at the native device rate. DeviceManager only connects
 * to this common interface, so adding a new transport does not touch the UI.
 */
class PowerDataSource : public QObject {
  Q_OBJECT

public:
  explicit PowerDataSource(QObject *parent = nullptr) : QObject(parent) {}
  ~PowerDataSource() override = default;

  virtual void start() = 0;
  virtual void stop() = 0;
  [[nodiscard]] virtual QString sourceName() const = 0;
  [[nodiscard]] virtual bool isConnected() const = 0;

signals:
  void sampleReceived(PowerData data);
  void connected(const QString &name);
  void disconnected();
};
