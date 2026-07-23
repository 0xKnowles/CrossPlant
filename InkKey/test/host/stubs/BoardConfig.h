#pragma once

// Host stub of the FreeInk BoardConfig: only the fields InkKey touches.

#include <cstdint>

namespace BoardConfig {

struct InputPins {
  int8_t back = 0, confirm = 1, left = 2, right = 3, up = 4, down = 5;
  int8_t power = 3;
  bool powerActiveHigh = false;
};

struct BoardProfile {
  InputPins input{};
};

inline BoardProfile ACTIVE{};

}  // namespace BoardConfig
