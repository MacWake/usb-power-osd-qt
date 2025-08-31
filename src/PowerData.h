#ifndef USB_POWER_OSD_POWERDATA_H
#define USB_POWER_OSD_POWERDATA_H

#include "qtypes.h"

struct PowerData {
  double voltage = 0.0;    // Volts
  double current = 0.0;    // Amperes
  double power = 0.0;      // Watts
  double energy = 0.0;     // Watt-hours
  quint64 timestamp = 0;   // Unix timestamp
};

#endif // USB_POWER_OSD_POWERDATA_H
