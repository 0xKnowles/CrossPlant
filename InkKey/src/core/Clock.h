#pragma once

// Unix-epoch time on top of the X3's DS3231 via freeink::Rtc.
//
// The RTC is kept in UTC — TOTP needs UTC and nothing else; local display time
// is deliberately out of scope. Between I2C reads the epoch is anchored to
// millis() so the UI can tick every second without hammering the bus; the
// anchor re-syncs from the RTC periodically and after any set().

#include <Rtc.h>

#include <cstddef>
#include <cstdint>

namespace inkkey {

class Clock {
 public:
  // Returns false when the DS3231 is missing (not an X3, dead coin cell, or
  // oscillator-stopped flag set, i.e. time was never programmed).
  bool begin();

  bool rtcPresent() const { return present_; }

  // True once the RTC holds a plausible time (year >= 2025).
  bool timeValid() const { return timeValid_; }

  // Current UTC unix time. 0 when the clock is not valid.
  uint64_t now();

  // Sets the RTC from broken-down UTC. Returns false on I2C failure.
  bool setUtc(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second);

  // Formats "YYYY-MM-DD HH:MM:SS UTC" of the current time into buf (>= 24 bytes).
  void formatUtc(char* buf, size_t bufLen);

  // Days-from-civil epoch conversion (public for tests/console).
  static uint64_t toEpoch(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second);

 private:
  bool syncFromRtc();

  freeink::Rtc rtc_;
  bool present_ = false;
  bool timeValid_ = false;
  uint64_t anchorEpoch_ = 0;
  unsigned long anchorMillis_ = 0;
  unsigned long lastSyncMillis_ = 0;

  static constexpr unsigned long RESYNC_INTERVAL_MS = 5UL * 60UL * 1000UL;
};

}  // namespace inkkey
