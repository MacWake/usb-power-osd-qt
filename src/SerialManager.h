#ifndef SERIALMANAGER_H
#define SERIALMANAGER_H

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
    ~SerialManager();
    
    void startScanning();
    void stopScanning();
    void requestData();

signals:
    void deviceConnected(const QString &deviceName);
    void deviceDisconnected();
    void dataReceived(const QByteArray &data);
    bool connectToDevice(const QSerialPortInfo &portInfo);

private slots:
    void onSerialDataReady();
    void onSerialError(QSerialPort::SerialPortError error);
    void scanForDevices();
    bool tryConnect(const QString &portName);

  private:
    bool isTargetDevice(const QSerialPortInfo &portInfo);
    // must set m_protocol and return true on success
    bool checkPLDProtocol();
    // must set m_protocol and return true on success
    bool checkMacwakeProtocol();

    QSerialPort *m_serialPort;
    QTimer *m_scanTimer;
    QByteArray m_readBuffer;
    bool m_isConnected = false;
    SerialProtocol m_protocol;

    // Known VID/PID for USB Power OSD devices
    static const quint16 TARGET_VENDOR_ID;
    static const quint16 TARGET_PRODUCT_ID;
};

#endif // SERIALMANAGER_H
