#ifndef SERIALMANAGER_H
#define SERIALMANAGER_H

#include "PowerData.h"
#include "PowerDataSource.h"

#include <QSerialPort>
#include <QSerialPortInfo>

enum SerialProtocol {
    PLD20 = 1,
    PLD28,
    MWAKE1
};

class SerialManager : public PowerDataSource
{
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

    /**
     * @brief Detect which PLD protocol a single serial line belongs to.
     *
     * @param line A trimmed serial line.
     * @param protocol Out parameter set to PLD20 or PLD28 on success.
     * @return true if the line matches a PLD protocol, false otherwise.
     */
    static bool detectPLDProtocol(const QByteArray &line, SerialProtocol &protocol);

    /**
     * @brief Parse the 8-hex-digit data portion of a PLD line.
     *
     * @param line A trimmed serial line (the first 8 hex chars are parsed).
     * @param protocol PLD20 or PLD28, used to select voltage/current quanta.
     * @param out Receives the parsed PowerData on success.
     * @return true if parsing succeeded, false otherwise.
     */
    static bool parsePLDLine(const QByteArray &line, SerialProtocol protocol,
                             PowerData &out);

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
