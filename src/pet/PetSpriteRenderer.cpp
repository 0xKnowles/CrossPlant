#include "PetSpriteRenderer.h"

#include <HalStorage.h>

#include "PetSpriteData.h"
#include "fontIds.h"
#include "PetState.h"
#include <Bitmap.h>
#include "PetManager.h"
#include "images/Seed144.h"
#include "images/BakedPlantSprites.h"

namespace {
static void drawScaledImage(GfxRenderer& renderer, const uint8_t* img, int x, int y, int width, int height, int scale) {
  if (scale <= 1) {
    renderer.drawImage(img, x, y, width, height);
    return;
  }
  for (int r = 0; r < height; r++) {
    for (int c = 0; c < width; c++) {
      int idx = (r * width + c) / 8;
      int bit = (img[idx] >> (7 - (c % 8))) & 1;
      if (bit == 0) {
        renderer.fillRect(x + c * scale, y + r * scale, scale, scale);
      }
    }
  }
}
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
    case PetStage::EGG:       return 0;
    case PetStage::HATCHLING: return 1;
    case PetStage::YOUNGSTER: return 2;
    case PetStage::COMPANION: return 3;
    case PetStage::ELDER:     return 4;
    case PetStage::DEAD:      return 5;
    default:                  return 0;
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
                                 uint8_t animFrame, bool forceHat, bool forceGlasses) {
  char path[80];
  bool drawn = false;

  // 1. Try BMP files first at display size!
  int size = displaySize(scale);
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
    if (stage == PetStage::EGG) {
      drawScaledImage(renderer, Seed, x, y, 144, 144, scale);
      drawn = true;
    } else {
      int stageNum = getStageNum(stage);
      const uint8_t* baked = getBakedPlantSprite(petType, stageNum);
      if (baked != nullptr) {
        drawScaledImage(renderer, baked, x, y, 144, 144, scale);
        drawn = true;
      }
    }
  }
  if (!drawn) {
    drawFallback(renderer, x, y, scale, stage, variant, petType, animFrame);
  }

  // Draw cosmetic overlays if equipped or forced
  if (stage != PetStage::EGG && stage != PetStage::DEAD) {
    bool drawHat = forceHat;
    bool drawGlasses = forceGlasses;
    bool drawCrown = false;
    bool drawScarf = false;

    PET_MANAGER.load();
    if (PET_MANAGER.exists() && PET_MANAGER.isAlive()) {
      const auto& petState = PET_MANAGER.getState();
      if (petState.equipHat) drawHat = true;
      if (petState.equipGlasses) drawGlasses = true;
      if (petState.equipCrown) drawCrown = true;
      if (petState.equipScarf) drawScarf = true;
    }

    const int cell = (scale == 1) ? 2 : 2 * scale; // size of logical pixel

    if (drawHat) {
      // Draw top hat (brim at row 4, crown at rows 0-3)
      for (int row = 0; row < 5; row++) {
        int startCol = (row == 4) ? 7 : 9;
        int endCol = (row == 4) ? 17 : 15;
        for (int col = startCol; col < endCol; col++) {
          renderer.fillRect(x + col * cell, y + row * cell, cell, cell);
        }
      }
    }

    if (drawGlasses) {
      // Draw round glasses at rows 7-9 (left lens cols 7-9, right lens cols 13-15, bridge cols 10-12)
      int glassRows[] = {7, 8, 9};
      for (int r : glassRows) {
        renderer.fillRect(x + 7 * cell, y + r * cell, cell, cell);
        renderer.fillRect(x + 9 * cell, y + r * cell, cell, cell);
        renderer.fillRect(x + 13 * cell, y + r * cell, cell, cell);
        renderer.fillRect(x + 15 * cell, y + r * cell, cell, cell);
      }
      renderer.fillRect(x + 8 * cell, y + 7 * cell, cell, cell);
      renderer.fillRect(x + 8 * cell, y + 9 * cell, cell, cell);
      renderer.fillRect(x + 14 * cell, y + 7 * cell, cell, cell);
      renderer.fillRect(x + 14 * cell, y + 9 * cell, cell, cell);
      // Bridge
      for (int col = 10; col <= 12; col++) {
        renderer.fillRect(x + col * cell, y + 8 * cell, cell, cell);
      }
    }

    if (drawCrown) {
      // Peaks
      renderer.fillRect(x + 9 * cell, y + 1 * cell, cell, cell);
      renderer.fillRect(x + 12 * cell, y + 1 * cell, cell, cell);
      renderer.fillRect(x + 15 * cell, y + 1 * cell, cell, cell);
      // Mid peaks
      renderer.fillRect(x + 9 * cell, y + 2 * cell, cell, cell);
      renderer.fillRect(x + 12 * cell, y + 2 * cell, cell, cell);
      renderer.fillRect(x + 15 * cell, y + 2 * cell, cell, cell);
      // Band base
      for (int col = 8; col <= 16; col++) {
        renderer.fillRect(x + col * cell, y + 3 * cell, cell, cell);
      }
    }

    if (drawScarf) {
      // Main neck band
      for (int col = 8; col <= 16; col++) {
        renderer.fillRect(x + col * cell, y + 12 * cell, cell, cell);
      }
      // Hanging tail
      renderer.fillRect(x + 15 * cell, y + 13 * cell, cell, cell);
      renderer.fillRect(x + 15 * cell, y + 14 * cell, cell, cell);
      renderer.fillRect(x + 15 * cell, y + 15 * cell, cell, cell);
    }
  }
}

void PetSpriteRenderer::drawMini(GfxRenderer& renderer, int x, int y, PetStage stage,
                                  PetMood mood, uint8_t variant, uint8_t petType) {
  // 1. Try BMP mini files first!
  if (drawBmpMini(renderer, x, y, MINI_W, stageName(stage), (int)variant, moodName(mood), petType, stage)) {
    return;
  }

  char path[88];
  // Try variant-specific mini file first
  if (variant > 0) {
    snprintf(path, sizeof(path), "/.crosspoint/pet/sprites/mini/%s_v%d_%s.bin",
             stageName(stage), (int)variant, moodName(mood));
    if (loadSprite(path, MINI_BYTES) == MINI_BYTES) {
      renderer.drawImage(spriteBuffer, x, y, MINI_W, MINI_H);
      return;
    }
  }
  // Default mini SD card path
  snprintf(path, sizeof(path), "/.crosspoint/pet/sprites/mini/%s_%s.bin", stageName(stage),
           moodName(mood));
  if (loadSprite(path, MINI_BYTES) == MINI_BYTES) {
    renderer.drawImage(spriteBuffer, x, y, MINI_W, MINI_H);
  } else {
    drawFallback(renderer, x, y, /*scale=*/1, stage, variant, petType, /*animFrame=*/0);
  }
}
