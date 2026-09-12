#ifndef TEST_SERIAL_PLD_PROTOCOL_H
#define TEST_SERIAL_PLD_PROTOCOL_H

#include <QObject>

class TestSerialPldProtocol : public QObject {
  Q_OBJECT

private slots:
  void detectPld20EightByteLine();
  void detectPld20WithFrameType();
  void detectPld28WithFrameType();
  void detectInvalidLines();

  void parsePld20Line();
  void parsePld28Line();
  void parsePld20WithFrameTypeByte();
  void parseLineTooShortFails();
  void parseInvalidProtocolFails();
};

#endif // TEST_SERIAL_PLD_PROTOCOL_H
