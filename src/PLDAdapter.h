#ifndef PLDADAPTER_H
#define PLDADAPTER_H

#include "PowerData.h"
#include "SerialAdapter.h"

#include <QSerialPort>

/**
 * @brief Adapter for the PLD serial protocol.
 *
 * Handles both the 20 V and 28 V frame variants in a single adapter.
 * The variant is detected from the optional 9th frame-type byte.
 */
class PLDAdapter : public SerialAdapter {
public:
  enum class FrameType { PLD20 = 1, PLD28 };

  [[nodiscard]] QString name() const override { return QStringLiteral("PLD"); }

  [[nodiscard]] bool detect(QSerialPort *port) override;
  [[nodiscard]] bool init(QSerialPort *port) override;
  [[nodiscard]] bool parseLine(const QByteArray &line, PowerData &out) override;

  /**
   * @brief Detect which PLD frame variant a single serial line belongs to.
   *
   * These helpers are public static members so they can be unit-tested
   * without instantiating a QSerialPort or a PLDAdapter.
   */
  static bool detectPLDProtocol(const QByteArray &line, FrameType &type);

  /**
   * @brief Parse the 8-hex-digit data portion of a PLD line.
   */
  static bool parsePLDLine(const QByteArray &line, FrameType type,
                           PowerData &out);

private:
  FrameType m_frameType = FrameType::PLD20;
};

#endif // PLDADAPTER_H
