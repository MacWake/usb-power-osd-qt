#include "PowerMonitor.h"
#include <QDateTime>
#include <QDebug>

PowerMonitor::PowerMonitor(QObject *parent)
    : QObject(parent)
{
}

void PowerMonitor::processBLEData(const QByteArray &data)
{
    parseV2BLEPacket(data);
}

void PowerMonitor::processSerialData(const QByteArray &data)
{
    // Process serial data - assuming similar format to BLE
    parseV2BLEPacket(data);
}

void PowerMonitor::parseV2BLEPacket(const QByteArray &data)
{
    if (data.size() < 12) { // Minimum expected packet size
        qDebug() << "Invalid packet size:" << data.size();
        return;
    }
    
    PowerData powerData;
    powerData.timestamp = QDateTime::currentMSecsSinceEpoch();
    
    // Parse packet assuming V2-BLE format
    // This is a placeholder - adjust based on actual V2-BLE protocol
    const quint8 *bytes = reinterpret_cast<const quint8*>(data.constData());
    
    // Voltage (assuming 16-bit value in millivolts at offset 0)
    quint16 voltage_mv = (bytes[1] << 8) | bytes[0];
    powerData.voltage = voltage_mv / 1000.0;
    
    // Current (assuming 16-bit value in milliamps at offset 2)
    quint16 current_ma = (bytes[3] << 8) | bytes[2];
    powerData.current = current_ma / 1000.0;
    
    // Power calculation
    powerData.power = powerData.voltage * powerData.current;
    
    // Energy accumulation
    if (m_lastTimestamp != 0) {
        double timeDelta = (powerData.timestamp - m_lastTimestamp) / 3600000.0; // Convert to hours
        m_energyAccumulator += powerData.power * timeDelta;
    }
    powerData.energy = m_energyAccumulator;
    
    m_lastData = powerData;
    m_lastTimestamp = powerData.timestamp;
    
    emit powerDataReceived(powerData);
}
