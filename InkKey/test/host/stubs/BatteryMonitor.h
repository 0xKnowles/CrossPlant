#pragma once

#include <cstdint>

class BatteryMonitor {
 public:
  BatteryMonitor() {}
  uint16_t readPercentage() const { return 87; }
  bool readPercentageChecked(uint16_t& out) const {
    out = 87;
    return true;
  }
};
