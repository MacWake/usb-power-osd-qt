// PDData.cpp

#include "PowerDelivery.h"

// Define the voltage values
const int PowerDelivery::pd_volts[PD_COUNT] = {0, 5, 9, 15, 20, 28, 36, 48};

bool PowerDelivery::within(int value, int base_value) {
  const int variance = static_cast<int>(static_cast<float>(base_value) * 0.20);
  return value >= base_value - variance && value <= base_value + variance;
}

// Get the PD_VOLTS value for a given millivolt
PowerDelivery::PD_VOLTS PowerDelivery::getEnum(float voltage) {
  auto millivolt = static_cast<int>(voltage * 1000);
  PD_VOLTS voltEnum = PD_NONE;
  if (within(millivolt, 5000)) {
    voltEnum = PD_5V;
  } else if (within(millivolt, 9000)) {
    voltEnum = PD_9V;
  } else if (within(millivolt, 15000)) {
    voltEnum = PD_15V;
  } else if (within(millivolt, 20000)) {
    voltEnum = PD_20V;
  } else if (within(millivolt, 28000)) {
    voltEnum = PD_28V;
  } else if (within(millivolt, 36000)) {
    voltEnum = PD_36V;
  } else if (within(millivolt, 48000)) {
    voltEnum = PD_48V;
  } else {
    voltEnum = PD_NONE;
  }
  return voltEnum;
}

int PowerDelivery::getVoltage(PD_VOLTS volt) { return pd_volts[volt]; }
