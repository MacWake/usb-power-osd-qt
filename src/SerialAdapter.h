#ifndef SERIALADAPTER_H
#define SERIALADAPTER_H

#include "PowerData.h"

#include <QByteArray>
#include <QSerialPort>
#include <QString>

/**
 * @brief Abstract interface for a serial-meter protocol adapter.
 *
 * Each adapter probes the meter, optionally initializes it by sending
 * commands, and parses incoming lines into PowerData samples.
 */
class SerialAdapter {
public:
  virtual ~SerialAdapter() = default;

  /**
   * @brief Human-readable adapter name (e.g. "PLD").
   */
  [[nodiscard]] virtual QString name() const = 0;

  /**
   * @brief Probe whether the meter connected to @p port speaks this protocol.
   *
   * Implementations may read lines from the port. On success the adapter
   * should store any detected variant internally so parseLine() can use it.
   */
  [[nodiscard]] virtual bool detect(QSerialPort *port) = 0;

  /**
   * @brief Initialize the meter after detection.
   *
   * This phase can be used to send configuration or start commands. The
   * default for adapters that need no init is to return true.
   */
  [[nodiscard]] virtual bool init(QSerialPort *port) = 0;

  /**
   * @brief Parse one trimmed line from the meter into a PowerData sample.
   */
  [[nodiscard]] virtual bool parseLine(const QByteArray &line,
                                       PowerData &out) = 0;

protected:
  /**
   * @brief Wait up to @p timeoutMs for a complete line to be available.
   */
  [[nodiscard]] static bool waitForLineAvailable(QSerialPort *port,
                                                  int timeoutMs);

  /**
   * @brief Read and trim the next available line, returning an empty array if
   * none is ready.
   */
  [[nodiscard]] static QByteArray readLineTrimmed(QSerialPort *port);
};

#endif // SERIALADAPTER_H
