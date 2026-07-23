#pragma once

// Host stub of the Arduino-ESP32 Preferences (NVS) API over an in-memory map,
// so vault create/save/unlock round-trips genuinely work in the simulator.

#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

class Preferences {
 public:
  static std::map<std::string, std::vector<uint8_t>> store;

  bool begin(const char*, bool = false) { return true; }
  void end() {}
  bool isKey(const char* key) { return store.count(key) > 0; }

  size_t getBytes(const char* key, void* buf, size_t maxLen) {
    auto it = store.find(key);
    if (it == store.end()) return 0;
    size_t n = it->second.size() < maxLen ? it->second.size() : maxLen;
    memcpy(buf, it->second.data(), n);
    return n;
  }
  size_t putBytes(const char* key, const void* buf, size_t len) {
    const uint8_t* p = static_cast<const uint8_t*>(buf);
    store[key] = std::vector<uint8_t>(p, p + len);
    return len;
  }
  uint32_t getUInt(const char* key, uint32_t defaultValue = 0) {
    auto it = store.find(key);
    if (it == store.end() || it->second.size() != 4) return defaultValue;
    uint32_t v;
    memcpy(&v, it->second.data(), 4);
    return v;
  }
  size_t putUInt(const char* key, uint32_t v) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&v);
    store[key] = std::vector<uint8_t>(p, p + 4);
    return 4;
  }
  bool clear() {
    store.clear();
    return true;
  }
};
