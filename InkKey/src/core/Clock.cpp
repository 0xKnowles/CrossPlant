#include "Clock.h"

#include <Arduino.h>

#include <cstdio>

namespace inkkey {

// Howard Hinnant's days-from-civil algorithm.
uint64_t Clock::toEpoch(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second) {
  int32_t y = year;
  int32_t m = month;
  int32_t d = day;
  y -= m <= 2;
  const int32_t era = (y >= 0 ? y : y - 399) / 400;
  const uint32_t yoe = uint32_t(y - era * 400);
  const uint32_t doy = uint32_t((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);
  const uint32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  const int64_t days = int64_t(era) * 146097 + int64_t(doe) - 719468;
  return uint64_t(days * 86400 + int64_t(hour) * 3600 + int64_t(minute) * 60 + int64_t(second));
}

bool Clock::begin() {
  present_ = rtc_.begin();
  if (present_) syncFromRtc();
  return present_;
}

bool Clock::syncFromRtc() {
  freeink::Rtc::DateTime dt;
  if (!rtc_.now(dt)) {
    timeValid_ = false;
    return false;
  }
  if (dt.year < 2025) {
    // Oscillator ran but the time was never programmed (DS3231 defaults to 2000).
    timeValid_ = false;
    return false;
  }
  anchorEpoch_ = toEpoch(dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
  anchorMillis_ = millis();
  lastSyncMillis_ = anchorMillis_;
  timeValid_ = true;
  return true;
}

uint64_t Clock::now() {
  if (!present_) return 0;
  unsigned long ms = millis();
  if (!timeValid_ || ms - lastSyncMillis_ >= RESYNC_INTERVAL_MS) {
    if (!syncFromRtc() && !timeValid_) return 0;
    ms = anchorMillis_;
  }
  return anchorEpoch_ + (ms - anchorMillis_) / 1000UL;
}

bool Clock::setUtc(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second) {
  if (!present_) return false;
  freeink::Rtc::DateTime dt;
  dt.year = year;
  dt.month = month;
  dt.day = day;
  dt.hour = hour;
  dt.minute = minute;
  dt.second = second;
  // Zeller-ish weekday from the epoch: day 0 (1970-01-01) was a Thursday (4).
  uint64_t days = toEpoch(year, month, day, 0, 0, 0) / 86400ULL;
  dt.weekday = uint8_t((days + 4) % 7);
  if (!rtc_.set(dt)) return false;
  return syncFromRtc();
}

void Clock::formatUtc(char* buf, size_t bufLen) {
  freeink::Rtc::DateTime dt;
  if (!present_ || !rtc_.now(dt)) {
    snprintf(buf, bufLen, "RTC unavailable");
    return;
  }
  snprintf(buf, bufLen, "%04u-%02u-%02u %02u:%02u:%02u UTC", dt.year, dt.month, dt.day, dt.hour, dt.minute,
           dt.second);
}

}  // namespace inkkey
