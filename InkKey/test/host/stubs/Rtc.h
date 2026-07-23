#pragma once

// Host stub of freeink::Rtc backed by a settable simulated wall clock that
// advances with the simulator's millis().

#include <cstdint>

unsigned long millis();

namespace freeink {

class Rtc {
 public:
  struct DateTime {
    uint16_t year = 2000;
    uint8_t month = 1;
    uint8_t day = 1;
    uint8_t hour = 0;
    uint8_t minute = 0;
    uint8_t second = 0;
    uint8_t weekday = 0;
  };

  // Simulator state (shared across instances, like the one real chip).
  static bool present;
  static bool timeSet;
  static DateTime current;
  static unsigned long setAtMillis;

  bool begin() { return present; }
  bool now(DateTime& out);
  bool set(const DateTime& dt) {
    current = dt;
    timeSet = true;
    setAtMillis = millis();
    return true;
  }
  bool adjust(int32_t, DateTime* = nullptr) { return false; }
};

}  // namespace freeink

using Rtc = freeink::Rtc;
