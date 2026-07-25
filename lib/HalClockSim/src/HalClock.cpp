#include "HalClock.h"

#include <cstdio>
#include <ctime>

HalClock halClock;

namespace {
constexpr const char* kMonthNames[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                       "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

// The host clock is always set, so unlike the hardware HAL there is no "never synced"
// state to guard against here.
bool hostTimeUtc(struct tm& out) {
  const std::time_t now = std::time(nullptr);
  if (now <= 0) return false;
#if defined(_WIN32)
  return gmtime_s(&out, &now) == 0;
#else
  return gmtime_r(&now, &out) != nullptr;
#endif
}
}  // namespace

bool HalClock::hasValidTime() const {
  struct tm t;
  return hostTimeUtc(t);
}

bool HalClock::getTime(uint8_t& hour, uint8_t& minute) const {
  struct tm t;
  if (!hostTimeUtc(t)) {
    hour = 0;
    minute = 0;
    return false;
  }
  hour = static_cast<uint8_t>(t.tm_hour);
  minute = static_cast<uint8_t>(t.tm_min);
  return true;
}

bool HalClock::getDateTime(uint16_t& year, uint8_t& month, uint8_t& day, uint8_t& hour, uint8_t& minute) const {
  struct tm t;
  if (!hostTimeUtc(t)) {
    year = 0;
    month = 0;
    day = 0;
    hour = 0;
    minute = 0;
    return false;
  }
  year = static_cast<uint16_t>(t.tm_year + 1900);
  month = static_cast<uint8_t>(t.tm_mon + 1);
  day = static_cast<uint8_t>(t.tm_mday);
  hour = static_cast<uint8_t>(t.tm_hour);
  minute = static_cast<uint8_t>(t.tm_min);
  return true;
}

bool HalClock::formatTime(char* buf, size_t bufSize, uint8_t utcOffsetQuarterHoursBiased, bool use12Hour) const {
  if (!buf || bufSize < (use12Hour ? 9u : 6u)) return false;
  uint8_t h, m;
  if (!getTime(h, m)) return false;

  if (utcOffsetQuarterHoursBiased > 104) utcOffsetQuarterHoursBiased = 104;
  const int offsetQuarterHours = static_cast<int>(utcOffsetQuarterHoursBiased) - 48;
  int totalMinutes = static_cast<int>(h) * 60 + static_cast<int>(m) + offsetQuarterHours * 15;
  totalMinutes = ((totalMinutes % 1440) + 1440) % 1440;

  const int hour24 = totalMinutes / 60;
  const int min = totalMinutes % 60;
  if (use12Hour) {
    const bool pm = hour24 >= 12;
    int hour12 = hour24 % 12;
    if (hour12 == 0) hour12 = 12;
    snprintf(buf, bufSize, "%d:%02d %s", hour12, min, pm ? "PM" : "AM");
  } else {
    snprintf(buf, bufSize, "%02d:%02d", hour24, min);
  }
  return true;
}

bool HalClock::formatDate(char* buf, size_t bufSize, uint8_t utcOffsetQuarterHoursBiased) const {
  if (!buf || bufSize < 13u) return false;

  // Resolve the offset through a Unix timestamp so month/year rollover comes from the
  // standard library rather than hand-rolled calendar maths.
  const std::time_t now = std::time(nullptr);
  if (now <= 0) return false;
  if (utcOffsetQuarterHoursBiased > 104) utcOffsetQuarterHoursBiased = 104;
  const int offsetQuarterHours = static_cast<int>(utcOffsetQuarterHoursBiased) - 48;
  const std::time_t shifted = now + offsetQuarterHours * 900;

  struct tm t;
#if defined(_WIN32)
  if (gmtime_s(&t, &shifted) != 0) return false;
#else
  if (gmtime_r(&shifted, &t) == nullptr) return false;
#endif

  snprintf(buf, bufSize, "%s %u, %u", kMonthNames[t.tm_mon], static_cast<unsigned int>(t.tm_mday),
           static_cast<unsigned int>(t.tm_year + 1900));
  return true;
}
