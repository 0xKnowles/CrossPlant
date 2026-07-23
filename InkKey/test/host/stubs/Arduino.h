#pragma once

// Host-build stub of the Arduino core: just enough surface for InkKey's
// firmware sources to compile and run in the host simulator (test/host).

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <string>

unsigned long millis();
void delay(unsigned long ms);

class HostSerial {
 public:
  std::deque<char> rx;      // bytes the simulated host "sent" to the device
  std::string tx;           // everything the firmware printed

  void begin(unsigned long) {}
  int available() { return int(rx.size()); }
  int read() {
    if (rx.empty()) return -1;
    char c = rx.front();
    rx.pop_front();
    return c;
  }
  void inject(const char* s) {
    for (const char* p = s; *p; p++) rx.push_back(*p);
  }

  void print(const char* s) { tx += s; }
  void print(char c) { tx += c; }
  void print(int v) { tx += std::to_string(v); }
  void print(unsigned v) { tx += std::to_string(v); }
  void print(uint8_t v) { tx += std::to_string(v); }
  void print(uint16_t v) { tx += std::to_string(v); }
  void println() { tx += '\n'; }
  template <typename T>
  void println(T v) {
    print(v);
    tx += '\n';
  }
};

extern HostSerial Serial;
