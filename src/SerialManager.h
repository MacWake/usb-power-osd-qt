#ifndef SERIALMANAGER_H
#define SERIALMANAGER_H

#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTimer>

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

private slots:
    void onSerialDataReady();
    void onSerialError(QSerialPort::SerialPortError error);
    void scanForDevices();

private:
    void connectToDevice(const QSerialPortInfo &portInfo);
    bool isTargetDevice(const QSerialPortInfo &portInfo);
    
    QSerialPort *m_serialPort;
    QTimer *m_scanTimer;
    QByteArray m_readBuffer;
    bool m_isConnected = false;
    
    // Known VID/PID for USB Power OSD devices
    static const quint16 TARGET_VENDOR_ID;
    static const quint16 TARGET_PRODUCT_ID;
};

#endif // SERIALMANAGER_H
