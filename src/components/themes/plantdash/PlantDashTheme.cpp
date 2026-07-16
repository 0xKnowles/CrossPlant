#include "PlantDashTheme.h"

#include <Bitmap.h>
#include <Epub.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
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
#include "activities/reader/ReadingStatsUtils.h"
#include "components/UITheme.h"
#include "components/icons/cover.h"
#include "components/icons/streak.h"
#include "fontIds.h"
#include "pet/PetEvolution.h"
#include "pet/PetManager.h"
#include "pet/PetSpriteRenderer.h"

namespace {
constexpr int kCardRadius = 12;
constexpr int kCardPad = 16;
constexpr int kCardGap = 14;
constexpr int kTilePadTop = 10;
// Reading card height must equal PlantDashTheme::kCoverImageHeight + kCardPad*2 so the cover
// thumbnail HomeActivity generates (see plantDashHomeCoverHeight in HomeActivity.cpp) exactly
// fills the card's cover slot with no extra scaling.
constexpr int kReadingCardH = PlantDashTheme::kCoverImageHeight + kCardPad * 2;
constexpr int kGardenCardH = 264;
constexpr int kGardenBandH = 36;
constexpr int kCoverW = PlantDashTheme::kCoverImageWidth;
constexpr int kProgressBarH = 12;
constexpr int kVitalBarH = 8;
constexpr int kSpriteSize = 148;
constexpr int kFooterIconSize = 24;

int cardInset(const GfxRenderer& renderer) { return renderer.getScreenWidth() >= 560 ? 50 : 20; }

void formatCompactDuration(const uint32_t seconds, char* buf, const size_t len) {
  if (seconds < 60) {
    snprintf(buf, len, "%s", tr(STR_STATS_LESS_THAN_MIN));
    return;
  }
  const uint32_t minutes = (seconds + 30u) / 60u;
  if (minutes < 60) {
    snprintf(buf, len, "%lu min", static_cast<unsigned long>(minutes));
    return;
  }
  const uint32_t hours = minutes / 60u;
  const uint32_t remainder = minutes % 60u;
  if (remainder == 0) {
    snprintf(buf, len, "%luh", static_cast<unsigned long>(hours));
  } else {
    snprintf(buf, len, "%luh %lum", static_cast<unsigned long>(hours), static_cast<unsigned long>(remainder));
  }
}

bool estimatedTimeLeft(const BookReadingStats& stats, const float progressPercent, uint32_t& seconds) {
  if (stats.estimatedTimeLeftSeconds > 0) {
    seconds = stats.estimatedTimeLeftSeconds;
    return true;
  }
  seconds = 0;
  if (progressPercent <= 0.0f || progressPercent >= 100.0f || stats.totalReadingSeconds < 120) {
    return false;
  }
  const float progress = progressPercent / 100.0f;
  const float estimate = (static_cast<float>(stats.totalReadingSeconds) * (1.0f - progress)) / progress;
  if (estimate <= 0.0f) {
    return false;
  }
  seconds = static_cast<uint32_t>(estimate + 0.5f);
  return seconds > 0;
}

Rect fittedBitmapRect(const Bitmap& bitmap, const Rect& target) {
  if (bitmap.getWidth() <= 0 || bitmap.getHeight() <= 0 || target.width <= 0 || target.height <= 0) {
    return target;
  }
  const float widthScale = static_cast<float>(target.width) / static_cast<float>(bitmap.getWidth());
  const float heightScale = static_cast<float>(target.height) / static_cast<float>(bitmap.getHeight());
  const float scale = std::min(1.0f, std::min(widthScale, heightScale));
  const int drawnW = std::min(target.width, std::max(1, static_cast<int>(std::ceil(bitmap.getWidth() * scale))));
  const int drawnH = std::min(target.height, std::max(1, static_cast<int>(std::ceil(bitmap.getHeight() * scale))));
  return Rect{target.x + (target.width - drawnW) / 2, target.y + (target.height - drawnH) / 2, drawnW, drawnH};
}

std::string coverPathForRect(const RecentBook& book, const Rect& imageRect) {
  if (book.coverBmpPath.empty()) {
    return {};
  }
  if (FsHelpers::hasEpubExtension(book.path)) {
    const std::string adaptivePath =
        Epub(book.path, "/.crosspoint").getAdaptiveThumbBmpPath(imageRect.width, imageRect.height);
    if (Storage.exists(adaptivePath.c_str())) {
      return adaptivePath;
    }
  }
  return UITheme::getCoverThumbPath(book.coverBmpPath, imageRect.width, imageRect.height);
}

void drawBookCover(const GfxRenderer& renderer, const Rect& coverRect, const RecentBook& book) {
  bool hasCover = false;
  const std::string coverBmpPath = coverPathForRect(book, coverRect);
  if (!coverBmpPath.empty() && Storage.exists(coverBmpPath.c_str())) {
    FsFile file;
    if (Storage.openFileForRead("HOME", coverBmpPath, file)) {
      Bitmap bitmap(file);
      if (bitmap.parseHeaders() == BmpReaderError::Ok) {
        const Rect bitmapRect = fittedBitmapRect(bitmap, coverRect);
        renderer.drawBitmap(bitmap, bitmapRect.x, bitmapRect.y, bitmapRect.width, bitmapRect.height);
        renderer.maskRoundedRectOutsideCorners(bitmapRect.x, bitmapRect.y, bitmapRect.width, bitmapRect.height,
                                               kCardRadius - 4, Color::White);
        renderer.drawRoundedRect(bitmapRect.x, bitmapRect.y, bitmapRect.width, bitmapRect.height, 1, kCardRadius - 4,
                                 true);
        hasCover = true;
      }
      file.close();
    }
  }

  if (!hasCover) {
    renderer.drawRoundedRect(coverRect.x, coverRect.y, coverRect.width, coverRect.height, 1, kCardRadius - 4, true);
    constexpr int iconSize = 32;
    renderer.drawIcon(CoverIcon, coverRect.x + (coverRect.width - iconSize) / 2, coverRect.y + 40, iconSize, iconSize);
    constexpr int textPadding = 12;
    const int textW = coverRect.width - textPadding * 2;
    const char* title = book.title.empty() ? book.path.c_str() : book.title.c_str();
    auto titleLines = renderer.wrappedText(UI_10_FONT_ID, title, textW, 4, EpdFontFamily::BOLD);
    const int lineH = renderer.getLineHeight(UI_10_FONT_ID);
    int textY = coverRect.y + (coverRect.height - static_cast<int>(titleLines.size()) * lineH) / 2;
    for (const auto& line : titleLines) {
      const int lineW = renderer.getTextWidth(UI_10_FONT_ID, line.c_str(), EpdFontFamily::BOLD);
      renderer.drawText(UI_10_FONT_ID, coverRect.x + (coverRect.width - lineW) / 2, textY, line.c_str(), true,
                        EpdFontFamily::BOLD);
      textY += lineH;
    }
  }
}

// Horizontal progress bar: dithered track with a solid rounded fill.
void drawProgressBar(const GfxRenderer& renderer, const int x, const int y, const int w, const int h,
                     const float percent) {
  const int radius = h / 2;
  renderer.fillRectDither(x, y, w, h, Color::LightGray);
  renderer.drawRoundedRect(x, y, w, h, 1, radius, true);
  const float clamped = std::clamp(percent, 0.0f, 100.0f);
  const int fillW = static_cast<int>(w * clamped / 100.0f + 0.5f);
  if (fillW >= h) {
    renderer.fillRoundedRect(x, y, fillW, h, radius, Color::Black);
  } else if (fillW > 0) {
    renderer.fillRect(x, y, fillW, h, true);
  }
}

// One cell of the 2x2 reading-stat grid: bold value over a small label.
void drawStatCell(const GfxRenderer& renderer, const int x, const int y, const int w, const char* value,
                  const char* label) {
  const int valueLineH = renderer.getLineHeight(UI_10_FONT_ID);
  renderer.drawText(UI_10_FONT_ID, x, y, value, true, EpdFontFamily::BOLD);
  const std::string visibleLabel = renderer.truncatedText(SMALL_FONT_ID, label, w);
  renderer.drawText(SMALL_FONT_ID, x, y + valueLineH, visibleLabel.c_str(), true);
}

int statGridHeight(const GfxRenderer& renderer) {
  const int cellH = renderer.getLineHeight(UI_10_FONT_ID) + renderer.getLineHeight(SMALL_FONT_ID);
  return cellH * 2 + 10;
}

void drawReadingCard(const GfxRenderer& renderer, const Rect& card, const std::vector<RecentBook>& recentBooks,
                     const BookReadingStats* stats, const float progressPercent, const char* currentChapterTitle) {
  if (recentBooks.empty()) {
    const int lineH = renderer.getLineHeight(UI_10_FONT_ID);
    constexpr int iconSize = 32;
    const int blockH = iconSize + 12 + lineH;
    const int iconY = card.y + (card.height - blockH) / 2;
    renderer.drawIcon(CoverIcon, card.x + (card.width - iconSize) / 2, iconY, iconSize, iconSize);
    const char* label = tr(STR_NO_RECENT_BOOKS);
    const int labelW = renderer.getTextWidth(UI_10_FONT_ID, label);
    renderer.drawText(UI_10_FONT_ID, card.x + (card.width - labelW) / 2, iconY + iconSize + 12, label, true);
    return;
  }

  const RecentBook& book = recentBooks[0];
  const Rect coverRect{card.x + kCardPad, card.y + kCardPad, kCoverW, card.height - kCardPad * 2};
  const int colX = coverRect.x + coverRect.width + kCardPad;
  const int colW = card.x + card.width - kCardPad - colX;

  // Title + chapter/author
  const char* title = book.title.empty() ? book.path.c_str() : book.title.c_str();
  auto titleLines = renderer.wrappedText(UI_12_FONT_ID, title, colW, 3, EpdFontFamily::BOLD);
  const int titleLineH = renderer.getLineHeight(UI_12_FONT_ID);
  const int smallLineH = renderer.getLineHeight(SMALL_FONT_ID);
  int textY = coverRect.y;
  for (const auto& line : titleLines) {
    renderer.drawText(UI_12_FONT_ID, colX, textY, line.c_str(), true, EpdFontFamily::BOLD);
    textY += titleLineH;
  }
  const char* subtitle =
      (currentChapterTitle != nullptr && currentChapterTitle[0] != '\0') ? currentChapterTitle : book.author.c_str();
  if (subtitle != nullptr && subtitle[0] != '\0') {
    textY += 4;
    auto subtitleLines = renderer.wrappedText(SMALL_FONT_ID, subtitle, colW, 2);
    for (const auto& line : subtitleLines) {
      renderer.drawText(SMALL_FONT_ID, colX, textY, line.c_str(), true);
      textY += smallLineH;
    }
  }

  // Progress bar + caption, positioned above the bottom-anchored stat grid.
  const int gridH = statGridHeight(renderer);
  const int gridY = coverRect.y + coverRect.height - gridH;
  const int barCaptionH = smallLineH + 6;
  const int barY = gridY - 18 - barCaptionH - kProgressBarH;
  char caption[32];
  if (progressPercent >= 0.0f) {
    drawProgressBar(renderer, colX, barY, colW, kProgressBarH, progressPercent);
    snprintf(caption, sizeof(caption), "%d%%", static_cast<int>(progressPercent + 0.5f));
  } else {
    drawProgressBar(renderer, colX, barY, colW, kProgressBarH, 0.0f);
    snprintf(caption, sizeof(caption), "-");
  }
  renderer.drawText(SMALL_FONT_ID, colX, barY + kProgressBarH + 6, caption, true);

  uint32_t estimatedSeconds = 0;
  const BookReadingStats emptyStats{};
  const BookReadingStats& bookStats = stats != nullptr ? *stats : emptyStats;
  if (estimatedTimeLeft(bookStats, progressPercent, estimatedSeconds) && !bookStats.isCompleted) {
    char timeLeft[32];
    char timeLeftLabel[48];
    formatCompactDuration(estimatedSeconds, timeLeft, sizeof(timeLeft));
    snprintf(timeLeftLabel, sizeof(timeLeftLabel), "%s %s", timeLeft, tr(STR_TIME_LEFT));
    const std::string visible = renderer.truncatedText(SMALL_FONT_ID, timeLeftLabel, colW - 60);
    const int w = renderer.getTextWidth(SMALL_FONT_ID, visible.c_str());
    renderer.drawText(SMALL_FONT_ID, colX + colW - w, barY + kProgressBarH + 6, visible.c_str(), true);
  }

  // 2x2 stat grid anchored to the card bottom.
  char value[40];
  const int cellW = (colW - 10) / 2;
  const int cellH = (gridH - 10) / 2;
  BookReadingStats::formatDuration(bookStats.totalReadingSeconds, value, sizeof(value));
  drawStatCell(renderer, colX, gridY, cellW, value, tr(STR_STATS_TIME_LBL));
  snprintf(value, sizeof(value), "%.1f",
           bookStats.totalReadingSeconds > 60
               ? static_cast<float>(bookStats.totalPagesTurned) * 60.0f / static_cast<float>(bookStats.totalReadingSeconds)
               : 0.0f);
  drawStatCell(renderer, colX + cellW + 10, gridY, cellW, value, tr(STR_STATS_PAGES_PER_MIN));
  snprintf(value, sizeof(value), "%u", static_cast<unsigned>(bookStats.sessionCount));
  drawStatCell(renderer, colX, gridY + cellH + 10, cellW, value, tr(STR_STATS_SESSIONS_LBL));
  const uint32_t avgSeconds = bookStats.sessionCount > 0 ? bookStats.totalReadingSeconds / bookStats.sessionCount : 0;
  BookReadingStats::formatDuration(avgSeconds, value, sizeof(value));
  drawStatCell(renderer, colX + cellW + 10, gridY + cellH + 10, cellW, value, tr(STR_STATS_AVG_SESSION_LBL));
}

// One labelled vital bar: "Moisture ......... 80%" over a slim rounded bar.
void drawVitalRow(const GfxRenderer& renderer, const int x, const int y, const int w, const char* label,
                  const uint8_t percent) {
  const int labelLineH = renderer.getLineHeight(SMALL_FONT_ID);
  char valueText[8];
  snprintf(valueText, sizeof(valueText), "%u%%", percent);
  const int valueW = renderer.getTextWidth(SMALL_FONT_ID, valueText);
  const std::string visibleLabel = renderer.truncatedText(SMALL_FONT_ID, label, w - valueW - 8);
  renderer.drawText(SMALL_FONT_ID, x, y, visibleLabel.c_str(), true);
  renderer.drawText(SMALL_FONT_ID, x + w - valueW, y, valueText, true);
  drawProgressBar(renderer, x, y + labelLineH + 2, w, kVitalBarH, static_cast<float>(percent));
}

void drawGardenCard(const GfxRenderer& renderer, const Rect& card) {
  // Inverted header band with rounded top corners.
  renderer.fillRoundedRect(card.x, card.y, card.width, kGardenBandH, kCardRadius, true, true, false, false,
                           Color::Black);
  const int bandTextH = renderer.getLineHeight(UI_10_FONT_ID);
  const int bandTextY = card.y + (kGardenBandH - bandTextH) / 2;
  renderer.drawText(UI_10_FONT_ID, card.x + kCardPad, bandTextY, tr(STR_VIRTUAL_PET), false, EpdFontFamily::BOLD);

  PET_MANAGER.load();
  const bool hasPlant = PET_MANAGER.exists() && PET_MANAGER.isAlive();

  const int bodyTop = card.y + kGardenBandH;
  const int smallLineH = renderer.getLineHeight(SMALL_FONT_ID);
  const int questY = card.y + card.height - kCardPad - smallLineH;

  if (!hasPlant) {
    const int lineH = renderer.getLineHeight(UI_10_FONT_ID);
    const int blockH = lineH + 6 + smallLineH;
    const int topY = bodyTop + (card.y + card.height - bodyTop - blockH) / 2;
    const char* headline = tr(STR_PET_NO_PET);
    const char* hint = tr(STR_PET_HATCH_EGG);
    const int headlineW = renderer.getTextWidth(UI_10_FONT_ID, headline, EpdFontFamily::BOLD);
    const int hintW = renderer.getTextWidth(SMALL_FONT_ID, hint);
    renderer.drawText(UI_10_FONT_ID, card.x + (card.width - headlineW) / 2, topY, headline, true, EpdFontFamily::BOLD);
    renderer.drawText(SMALL_FONT_ID, card.x + (card.width - hintW) / 2, topY + lineH + 6, hint, true);
    return;
  }

  const auto& state = PET_MANAGER.getState();
  const auto& farm = PET_MANAGER.getFarmState();
  const PetMood mood = PET_MANAGER.getMood();

  // Band right side: Dew balance, and active plot when more than one is owned.
  char bandRight[32];
  if (PET_MANAGER.ownedPlotCount() > 1) {
    snprintf(bandRight, sizeof(bandRight), "Plot %d/%d  %lu DD", PET_MANAGER.activePlotIndex() + 1,
             PET_MANAGER.ownedPlotCount(), static_cast<unsigned long>(farm.inkPoints));
  } else {
    snprintf(bandRight, sizeof(bandRight), "%lu DD", static_cast<unsigned long>(farm.inkPoints));
  }
  const int bandRightW = renderer.getTextWidth(UI_10_FONT_ID, bandRight, EpdFontFamily::BOLD);
  renderer.drawText(UI_10_FONT_ID, card.x + card.width - kCardPad - bandRightW, bandTextY, bandRight, false,
                    EpdFontFamily::BOLD);

  // Left: large plant sprite, vertically centred between band and quest line.
  const int spriteX = card.x + kCardPad + 2;
  const int spriteY = bodyTop + (questY - 8 - bodyTop - kSpriteSize) / 2;
  PetSpriteRenderer::drawPet(const_cast<GfxRenderer&>(renderer), spriteX, spriteY, state.stage, mood, 1,
                             state.evolutionVariant, state.petType, 0, false, false, kSpriteSize);

  // Right column: name, status/stage, then the four vital bars.
  const int colX = spriteX + kSpriteSize + kCardPad;
  const int colW = card.x + card.width - kCardPad - colX;
  const int nameLineH = renderer.getLineHeight(UI_10_FONT_ID);
  int rowY = bodyTop + 8;

  const char* petName = state.petName[0] ? state.petName : PetTypeNames::get(state.petType);
  const char* statusText = state.isSick          ? "PESTS!"
                           : state.attentionCall ? "NEEDS ATTENTION"
                                                 : PetEvolution::variantStageName(state.stage, state.evolutionVariant,
                                                                                  state.petType);
  const std::string visibleName = renderer.truncatedText(UI_10_FONT_ID, petName, colW - 4, EpdFontFamily::BOLD);
  renderer.drawText(UI_10_FONT_ID, colX, rowY, visibleName.c_str(), true, EpdFontFamily::BOLD);
  const std::string visibleStatus = renderer.truncatedText(SMALL_FONT_ID, statusText, colW - 4);
  renderer.drawText(SMALL_FONT_ID, colX, rowY + nameLineH + 2, visibleStatus.c_str(), true);
  rowY += nameLineH + 2 + smallLineH + 8;

  // Distribute the four vital rows across the space left above the quest line.
  const int vitalRowH = smallLineH + 2 + kVitalBarH;
  const int vitalsSpace = questY - 8 - rowY;
  const int vitalPitch = std::max(vitalRowH + 4, vitalsSpace / 4);
  drawVitalRow(renderer, colX, rowY, colW, tr(STR_PET_HUNGER), state.hunger);
  drawVitalRow(renderer, colX, rowY + vitalPitch, colW, tr(STR_PET_STAT_HAPPY), state.happiness);
  drawVitalRow(renderer, colX, rowY + vitalPitch * 2, colW, tr(STR_PET_HEALTH), state.health);
  drawVitalRow(renderer, colX, rowY + vitalPitch * 3, colW, tr(STR_PET_DISCIPLINE), state.discipline);

  // Quest line across the card bottom (mirrors the Dashboard footer quest text).
  char questLine[64];
  const bool pagesDone = farm.missionPagesRead >= 20;
  const bool tendsDone = farm.missionPetCount >= 3;
  if (pagesDone && tendsDone) {
    snprintf(questLine, sizeof(questLine), "Quest: Completed!");
  } else {
    snprintf(questLine, sizeof(questLine), "Quest: %u/20 pgs  %u/3 tends", farm.missionPagesRead,
             farm.missionPetCount);
  }
  const int questW = renderer.getTextWidth(SMALL_FONT_ID, questLine);
  renderer.drawText(SMALL_FONT_ID, card.x + (card.width - questW) / 2, questY, questLine, true);
}

void drawLifetimeFooter(const GfxRenderer& renderer, const Rect& rect, const int footerTop,
                        const GlobalReadingStats* globalStats) {
  char streakText[48];
  ReadingStatsDateTime today;
  const uint16_t streak = (globalStats != nullptr && getCurrentLocalReadingStatsDateTime(today))
                              ? globalStats->currentReadingStreak(&today.date)
                              : 0;
  if (streak > 0) {
    snprintf(streakText, sizeof(streakText), tr(STR_STATS_DAY_STREAK_FORMAT), static_cast<unsigned>(streak));
  } else {
    snprintf(streakText, sizeof(streakText), "%s", tr(STR_STATS_NO_STREAK));
  }

  char totalTime[24];
  BookReadingStats::formatDuration(globalStats != nullptr ? globalStats->totalReadingSeconds : 0, totalTime,
                                   sizeof(totalTime));
  char line[120];
  snprintf(line, sizeof(line), "%s  ·  %s  ·  %lu %s", streakText, totalTime,
           static_cast<unsigned long>(globalStats != nullptr ? globalStats->completedBooks : 0),
           tr(STR_STATS_COMPLETED_LBL));

  const int lineH = renderer.getLineHeight(UI_10_FONT_ID);
  const std::string visible = renderer.truncatedText(
      UI_10_FONT_ID, line, rect.width - kFooterIconSize - 8 - cardInset(renderer) * 2);
  const int textW = renderer.getTextWidth(UI_10_FONT_ID, visible.c_str());
  const int totalW = kFooterIconSize + 8 + textW;
  const int x = rect.x + (rect.width - totalW) / 2;
  const int centerY = footerTop + (rect.y + rect.height - footerTop) / 2;
  renderer.drawIcon(StreakIcon, x, centerY - kFooterIconSize / 2, kFooterIconSize, kFooterIconSize);
  renderer.drawText(UI_10_FONT_ID, x + kFooterIconSize + 8, centerY - lineH / 2, visible.c_str(), true);
}
}  // namespace

void PlantDashTheme::drawRecentBookCover(GfxRenderer& renderer, Rect rect, const std::vector<RecentBook>& recentBooks,
                                         int selectorIndex, bool& coverRendered, bool& coverBufferStored,
                                         bool& bufferRestored, const std::function<bool()>& storeCoverBuffer,
                                         const BookReadingStats* stats, const float progressPercent,
                                         const GlobalReadingStats* globalStats, const char* currentChapterTitle) const {
  (void)selectorIndex;
  (void)bufferRestored;

  const int inset = cardInset(renderer);
  const Rect readingCard{rect.x + inset, rect.y + kTilePadTop, renderer.getScreenWidth() - inset * 2, kReadingCardH};
  const Rect gardenCard{readingCard.x, readingCard.y + readingCard.height + kCardGap, readingCard.width, kGardenCardH};

  if (recentBooks.empty()) {
    renderer.drawRoundedRect(readingCard.x, readingCard.y, readingCard.width, readingCard.height, 1, kCardRadius,
                             true);
    drawReadingCard(renderer, readingCard, recentBooks, stats, progressPercent, currentChapterTitle);
    coverRendered = false;
    coverBufferStored = false;
  } else if (!coverRendered) {
    // Expensive part (SD bitmap decode) drawn once, then snapshotted: card
    // outline + cover only. Everything dynamic is drawn after the store so a
    // restored frame gets fresh stats/vitals on a clean background.
    renderer.drawRoundedRect(readingCard.x, readingCard.y, readingCard.width, readingCard.height, 1, kCardRadius,
                             true);
    const Rect coverRect{readingCard.x + kCardPad, readingCard.y + kCardPad, kCoverW,
                         readingCard.height - kCardPad * 2};
    drawBookCover(renderer, coverRect, recentBooks[0]);
    coverBufferStored = storeCoverBuffer();
    coverRendered = coverBufferStored;
  }

  if (!recentBooks.empty()) {
    drawReadingCard(renderer, readingCard, recentBooks, stats, progressPercent, currentChapterTitle);
  }

  renderer.drawRoundedRect(gardenCard.x, gardenCard.y, gardenCard.width, gardenCard.height, 1, kCardRadius, true);
  drawGardenCard(renderer, gardenCard);
  drawLifetimeFooter(renderer, rect, gardenCard.y + gardenCard.height, globalStats);
}
