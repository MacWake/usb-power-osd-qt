#ifndef POWERMONITOR_H
#define POWERMONITOR_H

#include <QObject>
#include "PowerData.h"

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
