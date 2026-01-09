#ifndef USB_POWER_OSD_POWERDATA_H
#define USB_POWER_OSD_POWERDATA_H

#include <cstdint>
#include <QMetaType>

struct PowerData {
  double voltage = 0.0;    // Volts
  double current = 0.0;    // Amperes
  double power = 0.0;      // Watts
  double energy = 0.0;     // Watt-hours
  uint64_t timestamp = 0;   // Unix timestamp
};

Q_DECLARE_METATYPE(PowerData)

#endif // USB_POWER_OSD_POWERDATA_H
