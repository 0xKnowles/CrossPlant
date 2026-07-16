#pragma once

#include <GfxRenderer.h>

#include "PetState.h"

// Renders pet sprites loaded from SD card at /.crosspoint/pet/sprites/
// Sprite format: raw 1-bit packed bitmap, MSB first per row, no header.
// Full sprites: 48x48 px = 288 bytes/frame
// Mini sprites: 24x24 px =  72 bytes/frame  (subfolder: mini/)
// Filename pattern: {stage}_{mood}.bin  e.g. hatchling_happy.bin
// Fallback: filled rect with stage initial letter when file is missing.
class PetSpriteRenderer {
 public:
  static constexpr int SPRITE_W    = 48;
  static constexpr int SPRITE_H    = 48;
  static constexpr int MINI_W      = 24;
  static constexpr int MINI_H      = 24;
  static constexpr int SPRITE_BYTES = SPRITE_W * SPRITE_H / 8;  // 288
  static constexpr int MINI_BYTES   = MINI_W * MINI_H / 8;       // 72

  // Draw sprite at (x,y). scale multiplies each logical pixel (default 1 = 48x48, 2 = 96x96).
  // variant selects evolution branch (0=default, 1=chubby, 2=misbehaved).
  // petType selects built-in pixel-art design (0=Chicken, 1=Cat, 2=Dog, 3=Dragon, 4=Bunny).
  // Tries {stage}_v{variant}_{mood}.bin first, falls back to {stage}_{mood}.bin, then built-in art.
  // targetSize > 0 overrides the drawn size and scales the baked/SD art to exactly that many
  // pixels (square), instead of centring the native 144px art inside displaySize(scale). Use it
  // when a caller wants the plant larger/smaller than the fixed scale steps. 0 keeps legacy sizing.
  static void drawPet(GfxRenderer& renderer, int x, int y, PetStage stage, PetMood mood,
                      int scale = 1, uint8_t variant = 0, uint8_t petType = 0,
                      uint8_t animFrame = 0, bool forceHat = false, bool forceGlasses = false,
                      int targetSize = 0);
  // Built-in grid is 24x24 logical pixels; each pixel renders as (2*scale) physical pixels.
  // At scale=3: 24*2*3 = 144px (same as old 12*4*3).
  static constexpr int BUILTIN_GRID = 24;
  static constexpr int displaySize(int scale = 1) { return BUILTIN_GRID * 2 * scale; }

  // Draw a mini sprite at (x,y), sized size x size (default MINI_W = 24). Falls back to pixel-art
  // if file missing. size != MINI_W scales the native 24x24 art to fit, for callers that need the
  // icon smaller to avoid crowding nearby text.
  static void drawMini(GfxRenderer& renderer, int x, int y, PetStage stage, PetMood mood,
                       uint8_t variant = 0, uint8_t petType = 0, int size = MINI_W);

  // Draw a static baked plant portrait (no BMP/SD lookup, no fallback pixel-art) at (x,y),
  // scaled to size x size. stage here is the raw baked-art stage index (see BakedPlantSprites.h),
  // not a PetStage — e.g. stage 3 = mature/companion art. Used by the boot screen, which just
  // wants a fixed portrait per species and doesn't have a live PetState to read a PetStage from.
  // No-op if petType/stage has no baked art.
  static void drawBakedPortrait(GfxRenderer& renderer, uint8_t petType, uint8_t stage, int x,
                                int y, int size);

  // Draw an arbitrary externally-supplied 144x144 baked 1-bit image (e.g. the Seed art in
  // Seed144.h) scaled to size x size at (x,y). Used when a caller wants a specific baked asset
  // directly rather than one selected via getBakedPlantSprite(petType, stage) — e.g. the boot
  // screen's seed/egg grid cell, which has no live PetState to derive a species+stage from.
  // No-op if img is null.
  static void drawBaked144Image(GfxRenderer& renderer, const uint8_t* img, int x, int y, int size);

  // Draw an arbitrary externally-supplied baked 1-bit image (MSB-first, 1=white/0=black, no row
  // padding) at (x,y) at its native width x height, no scaling. Used for baked assets that aren't
  // 144x144 (e.g. full-screen wallpapers), where the caller already sized the source art to match
  // the target exactly. No-op if img is null.
  static void drawBakedImageAt(GfxRenderer& renderer, const uint8_t* img, int x, int y, int width,
                               int height);

 private:
  // Shared 288-byte buffer — large enough for a full sprite frame.
  static uint8_t spriteBuffer[SPRITE_BYTES];

  static const char* stageName(PetStage stage);
  static const char* moodName(PetMood mood);
  static char stageInitial(PetStage stage);

  // Attempt to load sprite into spriteBuffer. Returns bytes read (0 on fail).
  static size_t loadSprite(const char* path, size_t expectedBytes);

  static void drawFallback(GfxRenderer& renderer, int x, int y, int scale, PetStage stage,
                           uint8_t variant = 0, uint8_t petType = 0, uint8_t animFrame = 0);
};
