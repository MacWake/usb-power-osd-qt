#include "SerialAdapter.h"

#include <QElapsedTimer>

bool SerialAdapter::waitForLineAvailable(QSerialPort *port, int timeoutMs) {
  QElapsedTimer timer;
  timer.start();
  while (timer.elapsed() < timeoutMs) {
    if (port->canReadLine()) {
      return true;
    }
    if (port->waitForReadyRead(10)) {
      // Data arrived; the next iteration will check canReadLine().
    }
  }
  return port->canReadLine();
}

QByteArray SerialAdapter::readLineTrimmed(QSerialPort *port) {
  QByteArray line = port->readLine();
  if (line.endsWith('\n')) {
    line = line.trimmed();
  }
  return line;
}
