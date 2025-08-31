#include "SerialManager.h"
#include <QDebug>

// These should match your USB Power OSD V2 device VID/PID
const quint16 SerialManager::TARGET_VENDOR_ID = 0x0483;  // STMicroelectronics
const quint16 SerialManager::TARGET_PRODUCT_ID = 0x5740; // Virtual COM Port

SerialManager::SerialManager(QObject *parent)
    : QObject(parent)
    , m_serialPort(new QSerialPort(this))
    , m_scanTimer(new QTimer(this))
{
    connect(m_serialPort, &QSerialPort::readyRead,
            this, &SerialManager::onSerialDataReady);
    connect(m_serialPort, &QSerialPort::errorOccurred,
            this, &SerialManager::onSerialError);
    
    // Scan for devices periodically
    m_scanTimer->setInterval(5000); // 5 seconds
    connect(m_scanTimer, &QTimer::timeout, this, &SerialManager::scanForDevices);
}

SerialManager::~SerialManager()
{
    if (m_serialPort->isOpen()) {
        m_serialPort->close();
    }
}

void SerialManager::startScanning()
{
    scanForDevices();
    m_scanTimer->start();
}

void SerialManager::stopScanning()
{
    m_scanTimer->stop();
}

void SerialManager::requestData()
{
    if (m_serialPort->isOpen()) {
        // Send request command - adjust based on your V2 protocol
        m_serialPort->write("GET\n");
    }
}

void SerialManager::scanForDevices()
{
    if (m_isConnected) {
        return;
    }
    
    const auto ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &portInfo : ports) {
        if (isTargetDevice(portInfo)) {
            qDebug() << "Found target serial device:" << portInfo.portName();
            connectToDevice(portInfo);
            return;
        }
    }
}

bool SerialManager::isTargetDevice(const QSerialPortInfo &portInfo)
{
    // Check VID/PID
    if (portInfo.hasVendorIdentifier() && portInfo.hasProductIdentifier()) {
        if (portInfo.vendorIdentifier() == TARGET_VENDOR_ID &&
            portInfo.productIdentifier() == TARGET_PRODUCT_ID) {
            return true;
        }
    }
    
    // Check description or manufacturer
    QString description = portInfo.description().toLower();
    QString manufacturer = portInfo.manufacturer().toLower();
    
    if (description.contains("usb power") || 
        description.contains("power osd") ||
        manufacturer.contains("stmicroelectronics")) {
        return true;
    }
    
    return false;
}

void SerialManager::connectToDevice(const QSerialPortInfo &portInfo)
{
    if (m_serialPort->isOpen()) {
        m_serialPort->close();
    }
    
    m_serialPort->setPort(portInfo);
    m_serialPort->setBaudRate(QSerialPort::Baud115200);
    m_serialPort->setDataBits(QSerialPort::Data8);
    m_serialPort->setParity(QSerialPort::NoParity);
    m_serialPort->setStopBits(QSerialPort::OneStop);
    m_serialPort->setFlowControl(QSerialPort::NoFlowControl);
    
    if (m_serialPort->open(QIODevice::ReadWrite)) {
        m_isConnected = true;
        emit deviceConnected(portInfo.portName());
        qDebug() << "Connected to serial device:" << portInfo.portName();
    } else {
        qDebug() << "Failed to open serial port:" << m_serialPort->errorString();
    }
}

void SerialManager::onSerialDataReady()
{
    QByteArray data = m_serialPort->readAll();
    m_readBuffer.append(data);
    
    // Process complete packets
    while (m_readBuffer.contains('\n')) {
        int index = m_readBuffer.indexOf('\n');
        QByteArray packet = m_readBuffer.left(index);
        m_readBuffer.remove(0, index + 1);
        
        if (!packet.isEmpty()) {
            emit dataReceived(packet);
        }
    }
}

void SerialManager::onSerialError(QSerialPort::SerialPortError error)
{
    if (error != QSerialPort::NoError) {
        qDebug() << "Serial port error:" << error;
        m_isConnected = false;
        emit deviceDisconnected();
        
        // Restart scanning after error
        QTimer::singleShot(2000, this, &SerialManager::startScanning);
    }
}
