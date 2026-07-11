#include "CoverPetTheme.h"

#include <Arduino.h>
#include <Bitmap.h>
#include <Epub.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalClock.h>
#include <HalGPIO.h>
#include <HalPowerManager.h>
#include <HalStorage.h>
#include <I18n.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "RecentBooksStore.h"
#include "activities/reader/BookReadingStats.h"
#include "activities/reader/GlobalReadingStats.h"
#include "components/UITheme.h"
#include "components/icons/cover.h"
#include "components/icons/folder24.h"
#include "components/icons/settings2.h"
#include "components/icons/book24.h"
#include "fontIds.h"
#include "pet/PetManager.h"
#include "pet/PetSpriteRenderer.h"
#include "pet/PetEvolution.h"

void CoverPetTheme::drawRecentBookCover(GfxRenderer& renderer, Rect rect, const std::vector<RecentBook>& recentBooks,
                                       int selectorIndex, bool& coverRendered, bool& coverBufferStored,
                                       bool& bufferRestored, const std::function<bool()>& storeCoverBuffer,
                                       const BookReadingStats* stats, float progressPercent,
                                       const GlobalReadingStats* globalStats, const char* currentChapterTitle) const {
  (void)selectorIndex;
  (void)globalStats;
  (void)currentChapterTitle;
  
  const int screenW = renderer.getScreenWidth();
  const int screenH = renderer.getScreenHeight();
  
  const bool hasBook = !recentBooks.empty();
  
  // ----------------------------------------------------
  // Top Half Layout (y: 60 to 365)
  // ----------------------------------------------------
  
  // Left Column (Book Cover & stats): X: 15 to 250 (width 235)
  const int col1Left = 15;
  const int col1Width = 235;
  const int col1CenterX = col1Left + col1Width / 2;
  
  // Draw Cover
  const int coverW = CoverPetMetrics::homeCoverImageWidth;
  const int coverH = CoverPetMetrics::homeCoverImageHeight;
  const int coverX = col1CenterX - coverW / 2;
  const int coverY = rect.y + 15; // y: 65
  
  if (hasBook) {
    if (!coverRendered && !bufferRestored) {
      const std::string coverPath = recentBooks[0].coverBmpPath;
      bool hasCover = false;
      if (!coverPath.empty()) {
        const std::string coverBmpPath = UITheme::getCoverThumbPath(coverPath, coverH);
        FsFile file;
        if (Storage.openFileForRead("HOME", coverBmpPath, file)) {
          Bitmap bitmap(file);
          if (bitmap.parseHeaders() == BmpReaderError::Ok) {
            renderer.drawBitmap(bitmap, coverX, coverY, coverW, coverH);
            hasCover = true;
          }
          file.close();
        }
      }
      
      // Draw cover border
      renderer.drawRect(coverX, coverY, coverW, coverH, true);
      
      if (!hasCover) {
        // Draw placeholder cover
        renderer.fillRect(coverX, coverY, coverW, coverH, false);
        renderer.drawRect(coverX, coverY, coverW, coverH, true);
        renderer.drawIcon(CoverIcon, coverX + (coverW - 32) / 2, coverY + (coverH - 32) / 2, 32, 32);
      }
      
      coverBufferStored = storeCoverBuffer();
      coverRendered = coverBufferStored;
    }
  } else {
    // Draw empty placeholder cover
    renderer.drawRect(coverX, coverY, coverW, coverH, true);
    renderer.drawIcon(CoverIcon, coverX + (coverW - 32) / 2, coverY + (coverH - 32) / 2, 32, 32);
  }
  
  // Draw stats underneath the cover
  const int statsY = coverY + coverH + 10; // y: 295
  if (hasBook) {
    // Progress
    char progBuf[32];
    if (progressPercent >= 0.0f) {
      snprintf(progBuf, sizeof(progBuf), "%d%% Read", (int)std::clamp(progressPercent, 0.0f, 100.0f));
    } else {
      snprintf(progBuf, sizeof(progBuf), "Unread");
    }
    renderer.drawCenteredText(SMALL_FONT_ID, statsY, progBuf, true);
    
    // Time spent reading
    if (stats && stats->sessionCount > 0) {
      char timeBuf[48];
      BookReadingStats::formatDuration(stats->totalReadingSeconds, timeBuf, sizeof(timeBuf));
      renderer.drawCenteredText(SMALL_FONT_ID, statsY + 18, timeBuf, true);
    } else {
      renderer.drawCenteredText(SMALL_FONT_ID, statsY + 18, "No stats", true);
    }
  } else {
    renderer.drawCenteredText(SMALL_FONT_ID, statsY, tr(STR_NO_OPEN_BOOK), true);
  }
  
  // Draw vertical separator line
  const int sepX = 260;
  renderer.drawLine(sepX, rect.y + 10, sepX, rect.y + 350, true);
  
  // Right Column (Pet Info): X: 275 to 513
  const int col2Left = 275;
  const int col2Width = 238;
  const int col2CenterX = col2Left + col2Width / 2;
  
  PET_MANAGER.load();
  if (PET_MANAGER.exists() && PET_MANAGER.isAlive()) {
    const auto& state = PET_MANAGER.getState();
    const PetMood mood = PET_MANAGER.getMood();
    const char* petName = state.petName[0] ? state.petName : PetTypeNames::get(state.petType);
    const char* stageName = PetEvolution::variantStageName(state.stage, state.evolutionVariant);
    
    // Title
    renderer.drawCenteredText(UI_10_FONT_ID, rect.y + 12, "MY PET", true, EpdFontFamily::BOLD);
    
    // Name & Stage
    char nameBuf[64];
    snprintf(nameBuf, sizeof(nameBuf), "%s (%s)", petName, stageName);
    renderer.drawCenteredText(SMALL_FONT_ID, rect.y + 35, nameBuf, true);
    
    const int petSpriteY = rect.y + 60;
    PetSpriteRenderer::drawPet(const_cast<GfxRenderer&>(renderer), col2CenterX - 48, petSpriteY, state.stage, mood,
                               2, state.evolutionVariant, state.petType);
                            
    // Draw vitals underneath the sprite
    const int vitalsY = petSpriteY + 60; // y: 170
    
    // Progress bar drawing lambda
    auto drawProgressBar = [&](const char* label, uint8_t value, int yPos) {
      renderer.drawText(SMALL_FONT_ID, col2Left, yPos, label, true);
      const int barX = col2Left;
      const int barY = yPos + 15;
      const int barW = col2Width;
      const int barH = 6;
      renderer.drawRect(barX, barY, barW, barH, true);
      const int fillW = (value * barW) / 100;
      if (fillW > 0) {
        renderer.fillRect(barX, barY, fillW, barH, true);
      }
    };
    
    drawProgressBar("Hunger", state.hunger, vitalsY);
    drawProgressBar("Happiness", state.happiness, vitalsY + 28);
    drawProgressBar("Health", state.health, vitalsY + 56);
    
    // Currency + Weight
    char balanceBuf[64];
    snprintf(balanceBuf, sizeof(balanceBuf), "%lu IP  |  Wt: %ug", (unsigned long)state.inkPoints, state.weight);
    renderer.drawCenteredText(SMALL_FONT_ID, vitalsY + 84, balanceBuf, true);
  } else {
    renderer.drawCenteredText(UI_10_FONT_ID, rect.y + 100, "NO PET ACTIVE", true);
    renderer.drawCenteredText(SMALL_FONT_ID, rect.y + 130, "Hatch a pet in settings!", true);
  }
  
  // ----------------------------------------------------
  // Bottom Half Layout (y: 370 to 735)
  // ----------------------------------------------------
  
  // Horizontal line separating top and bottom half
  renderer.drawLine(15, 365, screenW - 15, 365, true);
  
  const int bottomY = 380;
  const int bottomH = 350;
  const int margin = 20;
  const int gap = 15;
  const int cardW = (screenW - 2 * margin - gap) / 2;
  const int cardH = (bottomH - gap) / 2 - 25;
  
  auto drawCard = [&](int cardIdx, const char* label, const uint8_t* iconBmp, bool isPetSprite) {
    const int col = cardIdx % 2;
    const int row = cardIdx / 2;
    const int cardX = margin + col * (cardW + gap);
    const int cardY = bottomY + row * (cardH + gap);
    
    renderer.drawRoundedRect(cardX, cardY, cardW, cardH, 2, 8, true);
    
    const int centerY = cardY + cardH / 2;
    const int contentX = cardX + 15;
    
    if (isPetSprite) {
      PET_MANAGER.load();
      if (PET_MANAGER.exists() && PET_MANAGER.isAlive()) {
        const auto& pState = PET_MANAGER.getState();
        const PetMood pMood = PET_MANAGER.getMood();
        PetSpriteRenderer::drawMini(const_cast<GfxRenderer&>(renderer), contentX, centerY - 12, pState.stage, pMood,
                                    pState.evolutionVariant, pState.petType);
      } else {
        renderer.drawIcon(Book24Icon, contentX, centerY - 12, 24, 24);
      }
    } else if (iconBmp) {
      renderer.drawIcon(iconBmp, contentX, centerY - 12, 24, 24);
    }
    
    renderer.drawText(UI_10_FONT_ID, contentX + 32, centerY - renderer.getLineHeight(UI_10_FONT_ID) / 2, label, true, EpdFontFamily::BOLD);
  };
  
  drawCard(0, "Browse", Folder24Icon, false);
  drawCard(1, "Settings", Settings2Icon, false);
  drawCard(2, "My Pet", nullptr, true);
  drawCard(3, "Read", Book24Icon, false);
}
