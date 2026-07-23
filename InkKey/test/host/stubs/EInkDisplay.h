#pragma once

// Host stub of the FreeInk display facade. Holds a real 792x528 1-bit
// framebuffer; displayBuffer() hands the frame to the simulator via a hook so
// it can be written out as an image.

#include <cstdint>
#include <cstring>

class EInkDisplay {
 public:
  enum RefreshMode { FULL_REFRESH, HALF_REFRESH, FAST_REFRESH };

  static constexpr uint16_t W = 792;
  static constexpr uint16_t H = 528;
  static constexpr uint16_t WB = W / 8;

  void begin() { memset(fb_, 0xFF, sizeof(fb_)); }
  uint8_t* getFrameBuffer() { return fb_; }
  uint16_t getDisplayWidth() const { return W; }
  uint16_t getDisplayHeight() const { return H; }
  uint16_t getDisplayWidthBytes() const { return WB; }
  uint32_t getBufferSize() const { return sizeof(fb_); }

  void displayBuffer(RefreshMode = FAST_REFRESH, bool = false);
  void displayBufferAsync(RefreshMode = FAST_REFRESH) { displayBuffer(); }
  bool refreshBusy() { return false; }
  void deepSleep() {}

  // Simulator hook: called with the framebuffer on every displayBuffer().
  static void (*onPresent)(const uint8_t* fb);

 private:
  uint8_t fb_[WB * H];
};
