#include "PetSpriteRenderer.h"

#include <HalStorage.h>

#include <algorithm>

#include "PetSpriteData.h"
#include "fontIds.h"
#include "PetState.h"
#include <Bitmap.h>
#include "PetManager.h"
#include "images/Seed144.h"
#include "images/BakedPlantSprites.h"

namespace {
static const char* getSpeciesPrefix(uint8_t petType) {
  switch (petType) {
    case 0:  return "mon";
    case 1:  return "begonia";
    case 2:  return "alo";
    default: return "mon";
  }
}

static int getStageNum(PetStage stage) {
  switch (stage) {
    case PetStage::EGG:       return 1;
    case PetStage::HATCHLING: return 2;
    case PetStage::YOUNGSTER: return 3;
    case PetStage::COMPANION: return 4;
    case PetStage::ELDER:     return 4;
    case PetStage::DEAD:      return 5;
    default:                  return 1;
  }
}

static void drawBakedImage(GfxRenderer& renderer, const uint8_t* img, int x, int y, int width, int height) {
  for (int r = 0; r < height; r++) {
    for (int c = 0; c < width; c++) {
      int idx = (r * width + c) / 8;
      int bit = (img[idx] >> (7 - (c % 8))) & 1;
      if (bit == 0) {
        renderer.drawPixel(x + c, y + r);
      }
    }
  }
}

// Nearest-neighbour blit of a 1-bit baked image (srcW x srcH, MSB-first, row-packed) scaled to
// dstW x dstH. Used when a caller asks for a plant larger than the native art; at dst == src it
// produces the exact same pixels as drawBakedImage, so existing callers are unaffected.
static void drawBakedImageScaled(GfxRenderer& renderer, const uint8_t* img, int x, int y, int srcW,
                                 int srcH, int dstW, int dstH) {
  if (dstW <= 0 || dstH <= 0 || srcW <= 0 || srcH <= 0) {
    return;
  }
  for (int r = 0; r < dstH; r++) {
    const int sr = r * srcH / dstH;
    for (int c = 0; c < dstW; c++) {
      const int sc = c * srcW / dstW;
      const int idx = (sr * srcW + sc) / 8;
      const int bit = (img[idx] >> (7 - (sc % 8))) & 1;
      if (bit == 0) {
        renderer.drawPixel(x + c, y + r);
      }
    }
  }
}

static bool tryLoadBmp(GfxRenderer& renderer, const char* path, int x, int y, int size) {
  FsFile file;
  if (Storage.openFileForRead("HOME", path, file)) {
    Bitmap bitmap(file, true);
    if (bitmap.parseHeaders() == BmpReaderError::Ok) {
      renderer.drawBitmap(bitmap, x, y, size, size);
      file.close();
      return true;
    }
    file.close();
  }
  return false;
}

static bool drawBmpSprite(GfxRenderer& renderer, int x, int y, int size,
                          const char* stageStr, int variant, const char* mood,
                          uint8_t petType, PetStage stage) {
  char path[128];
  const char* prefix = getSpeciesPrefix(petType);
  int stageNum = getStageNum(stage);

  if (stage == PetStage::EGG) {
    if (tryLoadBmp(renderer, "/.crosspoint/pet/sprites/Seed.bmp", x, y, size)) return true;
    if (tryLoadBmp(renderer, "/.crosspoint/pet/sprites/egg.bmp", x, y, size)) return true;
    snprintf(path, sizeof(path), "/.crosspoint/pet/sprites/%s-0.bmp", prefix);
    if (tryLoadBmp(renderer, path, x, y, size)) return true;
  }

  if (stage == PetStage::DEAD) {
    if (tryLoadBmp(renderer, "/.crosspoint/pet/sprites/dead.bmp", x, y, size)) return true;
    snprintf(path, sizeof(path), "/.crosspoint/pet/sprites/%s-dead.bmp", prefix);
    if (tryLoadBmp(renderer, path, x, y, size)) return true;
    snprintf(path, sizeof(path), "/.crosspoint/pet/sprites/%s-5.bmp", prefix);
    if (tryLoadBmp(renderer, path, x, y, size)) return true;
  }

  if (variant > 0) {
    snprintf(path, sizeof(path), "/.crosspoint/pet/sprites/%s-%d_v%d_%s.bmp", prefix, stageNum, variant, mood);
    if (tryLoadBmp(renderer, path, x, y, size)) return true;
  }

  snprintf(path, sizeof(path), "/.crosspoint/pet/sprites/%s-%d_%s.bmp", prefix, stageNum, mood);
  if (tryLoadBmp(renderer, path, x, y, size)) return true;

  if (variant > 0) {
    snprintf(path, sizeof(path), "/.crosspoint/pet/sprites/%s-%d_v%d.bmp", prefix, stageNum, variant);
    if (tryLoadBmp(renderer, path, x, y, size)) return true;
  }

  snprintf(path, sizeof(path), "/.crosspoint/pet/sprites/%s-%d.bmp", prefix, stageNum);
  if (tryLoadBmp(renderer, path, x, y, size)) return true;

  if (variant > 0) {
    snprintf(path, sizeof(path), "/.crosspoint/pet/sprites/%s_v%d_%s.bmp", stageStr, variant, mood);
    if (tryLoadBmp(renderer, path, x, y, size)) return true;
  }
  snprintf(path, sizeof(path), "/.crosspoint/pet/sprites/%s_%s.bmp", stageStr, mood);
  if (tryLoadBmp(renderer, path, x, y, size)) return true;
  if (variant > 0) {
    snprintf(path, sizeof(path), "/.crosspoint/pet/sprites/%s_v%d.bmp", stageStr, variant);
    if (tryLoadBmp(renderer, path, x, y, size)) return true;
  }
  snprintf(path, sizeof(path), "/.crosspoint/pet/sprites/%s.bmp", stageStr);
  if (tryLoadBmp(renderer, path, x, y, size)) return true;

  return false;
}

static bool drawBmpMini(GfxRenderer& renderer, int x, int y, int size,
                        const char* stageStr, int variant, const char* mood,
                        uint8_t petType, PetStage stage) {
  char path[128];
  const char* prefix = getSpeciesPrefix(petType);
  int stageNum = getStageNum(stage);

  if (stage == PetStage::EGG) {
    if (tryLoadBmp(renderer, "/.crosspoint/pet/sprites/mini/Seed.bmp", x, y, size)) return true;
    if (tryLoadBmp(renderer, "/.crosspoint/pet/sprites/mini/egg.bmp", x, y, size)) return true;
    snprintf(path, sizeof(path), "/.crosspoint/pet/sprites/mini/%s-0.bmp", prefix);
    if (tryLoadBmp(renderer, path, x, y, size)) return true;
  }

  if (stage == PetStage::DEAD) {
    if (tryLoadBmp(renderer, "/.crosspoint/pet/sprites/mini/dead.bmp", x, y, size)) return true;
    snprintf(path, sizeof(path), "/.crosspoint/pet/sprites/mini/%s-dead.bmp", prefix);
    if (tryLoadBmp(renderer, path, x, y, size)) return true;
    snprintf(path, sizeof(path), "/.crosspoint/pet/sprites/mini/%s-5.bmp", prefix);
    if (tryLoadBmp(renderer, path, x, y, size)) return true;
  }

  if (variant > 0) {
    snprintf(path, sizeof(path), "/.crosspoint/pet/sprites/mini/%s-%d_v%d_%s.bmp", prefix, stageNum, variant, mood);
    if (tryLoadBmp(renderer, path, x, y, size)) return true;
  }

  snprintf(path, sizeof(path), "/.crosspoint/pet/sprites/mini/%s-%d_%s.bmp", prefix, stageNum, mood);
  if (tryLoadBmp(renderer, path, x, y, size)) return true;

  if (variant > 0) {
    snprintf(path, sizeof(path), "/.crosspoint/pet/sprites/mini/%s-%d_v%d.bmp", prefix, stageNum, variant);
    if (tryLoadBmp(renderer, path, x, y, size)) return true;
  }

  snprintf(path, sizeof(path), "/.crosspoint/pet/sprites/mini/%s-%d.bmp", prefix, stageNum);
  if (tryLoadBmp(renderer, path, x, y, size)) return true;

  if (variant > 0) {
    snprintf(path, sizeof(path), "/.crosspoint/pet/sprites/mini/%s_v%d_%s.bmp", stageStr, variant, mood);
    if (tryLoadBmp(renderer, path, x, y, size)) return true;
  }
  snprintf(path, sizeof(path), "/.crosspoint/pet/sprites/mini/%s_%s.bmp", stageStr, mood);
  if (tryLoadBmp(renderer, path, x, y, size)) return true;
  if (variant > 0) {
    snprintf(path, sizeof(path), "/.crosspoint/pet/sprites/mini/%s_v%d.bmp", stageStr, variant);
    if (tryLoadBmp(renderer, path, x, y, size)) return true;
  }
  snprintf(path, sizeof(path), "/.crosspoint/pet/sprites/mini/%s.bmp", stageStr);
  if (tryLoadBmp(renderer, path, x, y, size)) return true;

  return false;
}
} // namespace

// Static buffer shared across all sprite loads (saves heap, one-at-a-time use)
uint8_t PetSpriteRenderer::spriteBuffer[PetSpriteRenderer::SPRITE_BYTES];

// ---- Name helpers -------------------------------------------------------

const char* PetSpriteRenderer::stageName(PetStage stage) {
  switch (stage) {
    case PetStage::EGG:       return "egg";
    case PetStage::HATCHLING: return "hatchling";
    case PetStage::YOUNGSTER: return "youngster";
    case PetStage::COMPANION: return "companion";
    case PetStage::ELDER:     return "elder";
    case PetStage::DEAD:      return "dead";
    default:                  return "egg";
  }
}

const char* PetSpriteRenderer::moodName(PetMood mood) {
  switch (mood) {
    case PetMood::HAPPY:    return "happy";
    case PetMood::NEUTRAL:  return "neutral";
    case PetMood::SAD:      return "sad";
    case PetMood::SICK:     return "sick";
    case PetMood::SLEEPING: return "sleeping";
    case PetMood::DEAD:     return "dead";
    // Attention moods map to neutral sprite; indicator drawn by caller
    case PetMood::NEEDY:    return "neutral";
    case PetMood::REFUSING: return "neutral";
    default:                return "neutral";
  }
}

char PetSpriteRenderer::stageInitial(PetStage stage) {
  switch (stage) {
    case PetStage::EGG:       return 'E';
    case PetStage::HATCHLING: return 'H';
    case PetStage::YOUNGSTER: return 'Y';
    case PetStage::COMPANION: return 'C';
    case PetStage::ELDER:     return 'A';
    case PetStage::DEAD:      return 'X';
    default:                  return '?';
  }
}

// ---- File loading -------------------------------------------------------

size_t PetSpriteRenderer::loadSprite(const char* path, size_t expectedBytes) {
  size_t read = Storage.readFileToBuffer(path, reinterpret_cast<char*>(spriteBuffer),
                                         SPRITE_BYTES + 1, expectedBytes);
  return read;
}

// ---- Fallback renderer (pixel-art from PetSpriteData.h) -----------------

void PetSpriteRenderer::drawFallback(GfxRenderer& renderer, int x, int y, int scale,
                                     PetStage stage, uint8_t variant, uint8_t petType,
                                     uint8_t animFrame) {
  // 24x24 grid; each logical pixel = (2*scale) physical pixels
  const int cell = 2 * scale;
  const uint32_t* rows = getSpriteRows(stage, variant, petType, animFrame);
  for (int row = 0; row < 24; row++) {
    uint32_t mask = rows[row];
    for (int col = 0; col < 24; col++) {
      if (mask & (1u << (23 - col))) {
        renderer.fillRect(x + col * cell, y + row * cell, cell, cell);
      }
    }
  }
}

// ---- Public API ---------------------------------------------------------

void PetSpriteRenderer::drawPet(GfxRenderer& renderer, int x, int y, PetStage stage,
                                 PetMood mood, int scale, uint8_t variant, uint8_t petType,
                                 uint8_t animFrame, bool forceHat, bool forceGlasses,
                                 int targetSize) {
  char path[80];
  bool drawn = false;

  // 1. Try BMP files first at display size!
  // When targetSize is requested we render at exactly that many pixels and scale the native
  // 144px baked art to fill it, rather than centring the art inside the fixed displaySize(scale).
  const bool fillTarget = targetSize > 0;
  int size = fillTarget ? targetSize : displaySize(scale);
  if (drawBmpSprite(renderer, x, y, size, stageName(stage), (int)variant, moodName(mood), petType, stage)) {
    drawn = true;
  }

  // SD card sprites are 48x48 binary — only used at scale==1, no animFrame
  if (!drawn && variant > 0) {
    snprintf(path, sizeof(path), "/.crosspoint/pet/sprites/%s_v%d_%s.bin",
             stageName(stage), (int)variant, moodName(mood));
    if (loadSprite(path, SPRITE_BYTES) == SPRITE_BYTES && scale == 1) {
      renderer.drawImage(spriteBuffer, x, y, SPRITE_W, SPRITE_H);
      drawn = true;
    }
  }
  if (!drawn) {
    snprintf(path, sizeof(path), "/.crosspoint/pet/sprites/%s_%s.bin", stageName(stage),
             moodName(mood));
    if (loadSprite(path, SPRITE_BYTES) == SPRITE_BYTES && scale == 1) {
      renderer.drawImage(spriteBuffer, x, y, SPRITE_W, SPRITE_H);
      drawn = true;
    }
  }
  if (!drawn) {
    // Draw the native 144px baked art: scaled to fill when targetSize is set, otherwise centred
    // inside the size box exactly as before.
    auto blitBaked = [&](const uint8_t* baked) {
      if (fillTarget) {
        drawBakedImageScaled(renderer, baked, x, y, 144, 144, size, size);
      } else {
        const int dx = (size - 144) / 2;
        const int dy = (size - 144) / 2;
        drawBakedImage(renderer, baked, x + dx, y + dy, 144, 144);
      }
    };
    if (stage == PetStage::EGG) {
      if (petType == 0) {
        blitBaked(Seed);
        drawn = true;
      } else {
        const uint8_t* baked = getBakedPlantSprite(petType, 1);
        if (baked != nullptr) {
          blitBaked(baked);
          drawn = true;
        }
      }
    } else {
      int stageNum = getStageNum(stage);
      const uint8_t* baked = getBakedPlantSprite(petType, stageNum);
      if (baked != nullptr) {
        blitBaked(baked);
        drawn = true;
      }
    }
  }
  if (!drawn) {
    drawFallback(renderer, x, y, scale, stage, variant, petType, animFrame);
  }
}

void PetSpriteRenderer::drawMini(GfxRenderer& renderer, int x, int y, PetStage stage,
                                  PetMood mood, uint8_t variant, uint8_t petType, int size) {
  // 1. Try BMP mini files first! drawBitmap already scales to whatever size we ask for.
  if (drawBmpMini(renderer, x, y, size, stageName(stage), (int)variant, moodName(mood), petType, stage)) {
    return;
  }

  // Native art below is 24x24 (MINI_W x MINI_H); only scale it when a caller asked for a
  // different size, so the common case keeps the cheap 1:1 blit.
  const bool needsScale = (size != MINI_W);
  auto blitMini = [&](const uint8_t* img) {
    if (needsScale) {
      drawBakedImageScaled(renderer, img, x, y, MINI_W, MINI_H, size, size);
    } else {
      drawBakedImage(renderer, img, x, y, MINI_W, MINI_H);
    }
  };

  char path[88];
  // Try variant-specific mini file first
  if (variant > 0) {
    snprintf(path, sizeof(path), "/.crosspoint/pet/sprites/mini/%s_v%d_%s.bin",
             stageName(stage), (int)variant, moodName(mood));
    if (loadSprite(path, MINI_BYTES) == MINI_BYTES) {
      if (needsScale) {
        drawBakedImageScaled(renderer, spriteBuffer, x, y, MINI_W, MINI_H, size, size);
      } else {
        renderer.drawImage(spriteBuffer, x, y, MINI_W, MINI_H);
      }
      return;
    }
  }
  // Default mini SD card path
  snprintf(path, sizeof(path), "/.crosspoint/pet/sprites/mini/%s_%s.bin", stageName(stage),
           moodName(mood));
  if (loadSprite(path, MINI_BYTES) == MINI_BYTES) {
    if (needsScale) {
      drawBakedImageScaled(renderer, spriteBuffer, x, y, MINI_W, MINI_H, size, size);
    } else {
      renderer.drawImage(spriteBuffer, x, y, MINI_W, MINI_H);
    }
  } else {
    if (stage == PetStage::EGG) {
      if (petType > 0) {
        const uint8_t* bakedMini = getBakedPlantMiniSprite(petType, 1);
        if (bakedMini != nullptr) {
          blitMini(bakedMini);
          return;
        }
      }
    } else if (stage != PetStage::DEAD) {
      int stageNum = getStageNum(stage);
      const uint8_t* bakedMini = getBakedPlantMiniSprite(petType, stageNum);
      if (bakedMini != nullptr) {
        blitMini(bakedMini);
        return;
      }
    }
    drawFallback(renderer, x, y, /*scale=*/1, stage, variant, petType, /*animFrame=*/0);
  }
}
