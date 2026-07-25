#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "HalGPIO.h"

class HalClock;
extern HalClock halClock;  // Singleton

// Wall-clock abstraction, not a DS3231 driver.
//
// X3 has a battery-backed DS3231 whose time survives a full power cycle. X4 has no RTC
// chip at all, so it relies on the ESP32 system clock: set by NTP once WiFi is available,
// restored from the last-known-good timestamp at boot, and kept running across deep sleep
// by the SoC's own RTC timer. Callers that just need to know the current time should use
// hasValidTime() and the getters below, which work on both; hasBatteryBackedRtc() is only
// for the narrow case of "does this time survive being powered off".
class HalClock {
  bool _rtcPresent = false;
  bool _syncedThisSession = false;
  mutable uint8_t _cachedHour = 0;
  mutable uint8_t _cachedMinute = 0;
  mutable uint16_t _cachedYear = 2000;
  mutable uint8_t _cachedMonth = 1;
  mutable uint8_t _cachedDay = 1;
  mutable bool _hasCachedTime = false;
  mutable bool _hasCachedDate = false;
  mutable unsigned long _lastPollMs = 0;

  static constexpr unsigned long CLOCK_POLL_MS = 10000;  // 10 seconds

 public:
  // Call after gpio.begin() and powerManager.begin() (I2C already initialised for X3)
  void begin();

  // True only when a battery-backed DS3231 is present (X3), i.e. the wall clock survives
  // a full power cycle. Use this exclusively for decisions about clock *persistence* --
  // for "do we know what time it is right now", use hasValidTime(), which is also true on
  // RTC-less devices (X4) once their system clock has been set.
  bool hasBatteryBackedRtc() const { return _rtcPresent; }

  // True when the current wall-clock time is known and usable, from either the RTC chip
  // or a set system clock. False on an RTC-less device that has never synced and has no
  // restored timestamp -- in that state every getter below returns false.
  bool hasValidTime() const;

  // True once an NTP sync has succeeded during this boot. RTC-less devices lose their
  // clock on power-off, so callers use this (rather than a persisted "ever synced" flag)
  // to decide whether this boot still needs a sync.
  bool hasSyncedThisSession() const { return _syncedThisSession; }

  // Get current hour (0-23) and minute (0-59). Returns false if the time is unknown.
  bool getTime(uint8_t& hour, uint8_t& minute) const;

  // Format time into a caller-provided buffer.
  // 24h mode produces "HH:MM" (needs >=6 bytes); 12h mode produces "H:MM AM"/"HH:MM PM" (needs >=9 bytes).
  // utcOffsetQuarterHoursBiased: biased quarter-hour offset (48 = UTC+0, 0 = UTC-12, 104 = UTC+14).
  // use12Hour: when true, format as 12-hour clock with AM/PM suffix.
  // Returns false if the time is unknown.
  bool formatTime(char* buf, size_t bufSize, uint8_t utcOffsetQuarterHoursBiased = 48, bool use12Hour = false) const;

  // Returns the raw UTC date/time before any user-configured timezone offset is applied.
  // Both the DS3231 and the NTP-synced system clock are kept in UTC, so callers that need
  // wall-clock local time should apply SETTINGS.clockUtcOffsetQ.
  bool getDateTime(uint16_t& year, uint8_t& month, uint8_t& day, uint8_t& hour, uint8_t& minute) const {
    return getDate(year, month, day, hour, minute);
  }

  // Format date into a caller-provided buffer as "Mon D, YYYY".
  // utcOffsetQuarterHoursBiased matches formatTime so the date rolls over at local midnight.
  // Returns false if the time is unknown or the date is invalid.
  bool formatDate(char* buf, size_t bufSize, uint8_t utcOffsetQuarterHoursBiased = 48) const;

  // Sync the system clock from an NTP server, and additionally write it to the DS3231
  // when one is present so it survives a power cycle. Requires WiFi to be connected.
  // Blocks for up to ~5s while waiting for SNTP response. Returns true once the system
  // clock is set, including on devices with no RTC to persist it.
  //
  // Debouncing (skip if already synced) is enforced by the caller, not here, so the HAL
  // stays free of any app-layer settings dependency.
  bool syncFromNTP();

  // Current UTC time as a Unix timestamp for the caller to persist across a power cycle,
  // or 0 when there is nothing worth saving (unknown time, or an RTC that already keeps
  // its own). Pair with restorePersistedTime() at boot.
  uint32_t epochForPersistence() const;

  // Seeds the system clock from a timestamp saved before the last power-off, so RTC-less
  // devices boot with an approximately-correct clock instead of 1970. No-op when an RTC
  // is present or the system clock is already set (e.g. it survived deep sleep). The
  // restored time is stale by however long the device was powered off, so it is a
  // best-effort floor until an NTP sync corrects it.
  void restorePersistedTime(uint32_t savedEpoch);

 private:
  bool getDate(uint16_t& year, uint8_t& month, uint8_t& day, uint8_t& hour, uint8_t& minute) const;
  bool writeDateTimeToRTC(uint16_t year, uint8_t month, uint8_t day, uint8_t weekday, uint8_t hour, uint8_t minute,
                          uint8_t second);
};
