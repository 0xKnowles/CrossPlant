#pragma once

#include <cstdlib>

inline void esp_fill_random(void* buf, size_t len) {
  unsigned char* p = static_cast<unsigned char*>(buf);
  for (size_t i = 0; i < len; i++) p[i] = static_cast<unsigned char>(rand());
}
