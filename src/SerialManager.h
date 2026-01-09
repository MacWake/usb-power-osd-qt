#ifndef SERIALMANAGER_H
#define SERIALMANAGER_H

#include "PowerData.h"

#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTimer>

enum SerialProtocol {
    PLD20 = 1,
    PLD28,
    MWAKE1
};

class SerialManager : public QObject
{
    Q_OBJECT

public:
    explicit SerialManager(QObject *parent = nullptr);
    ~SerialManager() override;
    
    Q_INVOKABLE bool connectSerialDevice(const QSerialPortInfo &portInfo);
    Q_INVOKABLE void disconnect();

signals:
    void deviceConnected(const QString &deviceName);
    void deviceDisconnected();
    void dataReceived(PowerData data);

public slots:
    bool tryConnect(const QString &portName);

private slots:
    void onSerialDataReady();
    void onSerialError(QSerialPort::SerialPortError error);
    bool waitForLineAvailable(int timeoutMs);

  private:
    // must set m_protocol and return true on success
    bool checkPLDProtocol();
    // must set m_protocol and return true on success
    bool checkMacwakeProtocol();

    QSerialPort *m_serialPort;
    QByteArray m_readBuffer;
    bool m_isConnected = false;
    SerialProtocol m_protocol;

    // Known VID/PID for USB Power OSD devices
    static const quint16 TARGET_VENDOR_ID;
    static const quint16 TARGET_PRODUCT_ID;
};

#endif // SERIALMANAGER_H
