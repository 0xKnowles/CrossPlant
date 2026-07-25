#pragma once

#include <cstddef>
#include <cstdint>

class HalClock;
extern HalClock halClock;

// Simulator clock. Mirrors an RTC-less device (X4): no battery-backed chip, but the host
// system clock backs every getter, so clock-dependent screens can actually be exercised
// in the simulator instead of always taking the "no clock" fallback path.
class HalClock {
 public:
  void begin() {}
  bool hasBatteryBackedRtc() const { return false; }
  bool hasValidTime() const;
  bool hasSyncedThisSession() const { return false; }
  bool getTime(uint8_t& hour, uint8_t& minute) const;
  bool getDateTime(uint16_t& year, uint8_t& month, uint8_t& day, uint8_t& hour, uint8_t& minute) const;
  bool formatTime(char* buf, size_t bufSize, uint8_t utcOffsetQuarterHoursBiased = 48, bool use12Hour = false) const;
  bool formatDate(char* buf, size_t bufSize, uint8_t utcOffsetQuarterHoursBiased = 48) const;
  bool syncFromNTP() { return false; }
  uint32_t epochForPersistence() const { return 0; }
  void restorePersistedTime(uint32_t savedEpoch) { (void)savedEpoch; }
};
