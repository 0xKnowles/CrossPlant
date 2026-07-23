#pragma once

// Host stub of the FreeInk InputManager. The simulator queues button presses
// with hostPressButton(); each update() pops one press so every loop()
// iteration sees at most one clean press edge, like debounced hardware.

#include <cstdint>
#include <deque>

void hostPressButton(uint8_t button);

class InputManager {
 public:
  static constexpr uint8_t BTN_BACK = 0;
  static constexpr uint8_t BTN_CONFIRM = 1;
  static constexpr uint8_t BTN_LEFT = 2;
  static constexpr uint8_t BTN_RIGHT = 3;
  static constexpr uint8_t BTN_UP = 4;
  static constexpr uint8_t BTN_DOWN = 5;
  static constexpr uint8_t BTN_POWER = 6;
  static constexpr int POWER_BUTTON_PIN = 3;

  void begin() {}
  void update();

  bool isPressed(uint8_t) const { return false; }
  bool wasPressed(uint8_t button) const { return pressedEdge_ == button; }
  bool wasAnyPressed() const { return pressedEdge_ >= 0; }
  bool wasReleased(uint8_t) const { return false; }
  bool wasAnyReleased() const { return false; }
  bool isPowerButtonPressed() const { return false; }
  unsigned long getHeldTime() const { return 0; }
  unsigned long getPowerButtonHeldTime() const { return 0; }

 private:
  int pressedEdge_ = -1;
  friend void hostInputPop(InputManager&);
};
