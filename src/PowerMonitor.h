#ifndef POWERMONITOR_H
#define POWERMONITOR_H

#include <QObject>

struct PowerData {
    double voltage = 0.0;    // Volts
    double current = 0.0;    // Amperes
    double power = 0.0;      // Watts
    double energy = 0.0;     // Watt-hours
    quint64 timestamp = 0;   // Unix timestamp
};

class PowerMonitor : public QObject
{
    Q_OBJECT

public:
    explicit PowerMonitor(QObject *parent = nullptr);
    
    void processBLEData(const QByteArray &data);
    void processSerialData(const QByteArray &data);

signals:
    void powerDataReceived(const PowerData &data);

private:
    void parseV2BLEPacket(const QByteArray &data);
    PowerData m_lastData;
    double m_energyAccumulator = 0.0;
    quint64 m_lastTimestamp = 0;
};

#endif // POWERMONITOR_H
