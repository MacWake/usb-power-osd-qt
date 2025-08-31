#pragma once

#include <string>

// The PowerDelivery class encapsulates the PD_VOLTS enum and related
// functionality
class PowerDelivery {
public:
  // Enum defining power delivery voltage options
  enum PD_VOLTS {
    PD_NONE = 0,
    PD_5V = 1,
    PD_9V = 2,
    PD_15V = 3,
    PD_20V = 4,
    PD_28V = 5,
    PD_36V = 6,
    PD_48V = 7,
    PD_COUNT = 8,
  };

  static bool within(int value, int base_value);

  // Convert from millivolts to corresponding PD_VOLTS enum value
  static PD_VOLTS getEnum(float voltage);

  // Get the voltage value in volts corresponding to a given PD_VOLTS value
  static int getVoltage(PD_VOLTS volt);

private:
  // Color definitions corresponding to `PD_VOLTS`
  static const std::string colors[PD_COUNT];

  // Voltage values corresponding to `PD_VOLTS`
  static const int pd_volts[PD_COUNT];
};
