#include "SleepActivity.h"

#include <Bitmap.h>
#include <Epub.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalClock.h>
#include <HalStorage.h>
#include <I18n.h>
#include <PNGdec.h>
#include <Xtc.h>

#include <algorithm>
#include <cstdint>
#include <new>

#include "../home/RecentBookProgress.h"
#include "../reader/BookStatsView.h"
#include "../reader/EpubReaderActivity.h"
#include "../reader/EpubReaderUtils.h"
#include "../reader/TxtReaderActivity.h"
#include "../reader/XtcReaderActivity.h"
#include "AppVersion.h"
#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "RecentBooksStore.h"
#include "SleepCoverAssets.h"
#include "activities/reader/ReaderUtils.h"
#include "components/UITheme.h"
#include "components/themes/dashboard/DashboardTheme.h"
#include "components/themes/minimal/MinimalTheme.h"
#include "fontIds.h"
#include "images/Logo120.h"
#include "images/Seed144.h"
#include "images/MoonIcon.h"
#include "pet/PetEvolution.h"
#include "pet/PetManager.h"
#include "pet/PetSpriteRenderer.h"

namespace {

int statsBlockHeight(const GfxRenderer& renderer) {
  const int valueLineH = renderer.getLineHeight(UI_12_FONT_ID);
  const int labelLineH = renderer.getLineHeight(SMALL_FONT_ID);
  return valueLineH + 1 + labelLineH;
}

int statsBlockTop(const Rect& coverRect, const int index, const int blockH, const int rowCount) {
  const int remainingH = std::max(0, coverRect.height - blockH * rowCount);
  const int gapCount = rowCount - 1;
  const int gap = gapCount > 0 ? remainingH / gapCount : 0;
  const int remainder = gapCount > 0 ? remainingH % gapCount : 0;
  return coverRect.y + index * (blockH + gap) + std::min(index, remainder);
}

void drawRightAlignedText(const GfxRenderer& renderer, const int fontId, const int rightX, const int y,
                          const char* text, const bool bold = false, const bool black = true) {
  const EpdFontFamily::Style style = bold ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;
  const int width = renderer.getTextWidth(fontId, text, style);
  renderer.drawText(fontId, rightX - width, y, text, black, style);
}

void drawStatsRow(const GfxRenderer& renderer, const int rightX, const int y, const char* value, const char* label,
                  const bool black = true) {
  const int valueLineH = renderer.getLineHeight(UI_12_FONT_ID);
  drawRightAlignedText(renderer, UI_12_FONT_ID, rightX, y, value, true, black);
  drawRightAlignedText(renderer, SMALL_FONT_ID, rightX, y + valueLineH + 1, label, false, black);
}

constexpr bool TURN_OFF_SCREEN_AFTER_SLEEP_REFRESH = true;
constexpr int sleepBuildInfoSideMargin = 20;

bool sleepCoverFilterInvertsGeneratedScreen() {
  return SETTINGS.sleepScreenCoverFilter == CrossPointSettings::SLEEP_SCREEN_COVER_FILTER::INVERTED_BLACK_AND_WHITE;
}

void hideOverlayBatteryStrip(const GfxRenderer& renderer) {
  if (!SETTINGS.statusBarBattery) {
    return;
  }

  const ThemeMetrics& metrics = UITheme::getInstance().getMetrics();
  int orientedMarginTop, orientedMarginRight, orientedMarginBottom, orientedMarginLeft;
  renderer.getOrientedViewableTRBL(&orientedMarginTop, &orientedMarginRight, &orientedMarginBottom,
                                   &orientedMarginLeft);

  const int statusBarHeight = UITheme::getInstance().getStatusBarHeight();
  if (statusBarHeight <= 0) {
    return;
  }

  const int textY = renderer.getScreenHeight() - statusBarHeight - orientedMarginBottom - 4;
  const bool showBatteryPercentage =
      SETTINGS.hideBatteryPercentage == CrossPointSettings::HIDE_BATTERY_PERCENTAGE::HIDE_NEVER;

  // Reserve the full left-side status indicator lane used by bookmark + battery.
  // This keeps chapter/progress text readable while removing the battery glance target.
  static constexpr int bookmarkReserveWidth = 13;  // bookmark width + gap from BaseTheme::drawStatusBar()
  static constexpr int batteryPercentSpacing = 4;  // matches BaseTheme::batteryPercentSpacing
  const int clearWidth =
      bookmarkReserveWidth + metrics.batteryWidth +
      (showBatteryPercentage ? batteryPercentSpacing + renderer.getTextWidth(SMALL_FONT_ID, "100%") : 0);
  const int clearHeight = std::max(renderer.getTextHeight(SMALL_FONT_ID), metrics.batteryHeight + 6);

  renderer.fillRect(metrics.statusBarHorizontalMargin + orientedMarginLeft + 1, textY, clearWidth, clearHeight, false);
}

// Context passed through PNGdec's decode() user-pointer to the per-scanline draw callback.
struct PngOverlayCtx {
  const GfxRenderer* renderer;
  int screenW;
  int screenH;
  int srcWidth;
  int dstWidth;
  int dstX;
  int dstY;
  float yScale;
  int lastDstY;
  // Color-key transparency (tRNS chunk) for TRUECOLOR and GRAYSCALE images.
  // Initialized lazily on the first draw callback because tRNS is processed during decode(),
  // not during open() — so hasAlpha()/getTransparentColor() are only valid once decode() starts.
  // -2 = not yet read; -1 = no color key; >=0 = 0x00RRGGBB (TRUECOLOR) or low-byte gray.
  int32_t transparentColor;
  PNG* pngObj;  // for lazy-init of transparentColor on first callback
};

// PNGdec file I/O callbacks — mirror the pattern in PngToFramebufferConverter.cpp.
void* pngSleepOpen(const char* filename, int32_t* size) {
  FsFile* f = new FsFile();
  if (!Storage.openFileForRead("SLP", std::string(filename), *f)) {
    delete f;
    return nullptr;
  }
  *size = f->size();
  return f;
}
void pngSleepClose(void* handle) {
  FsFile* f = reinterpret_cast<FsFile*>(handle);
  if (f) {
    f->close();
    delete f;
  }
}
int32_t pngSleepRead(PNGFILE* pFile, uint8_t* pBuf, int32_t len) {
  FsFile* f = reinterpret_cast<FsFile*>(pFile->fHandle);
  return f ? f->read(pBuf, len) : 0;
}
int32_t pngSleepSeek(PNGFILE* pFile, int32_t pos) {
  FsFile* f = reinterpret_cast<FsFile*>(pFile->fHandle);
  if (!f) return -1;
  return f->seek(pos);
}

// Per-scanline draw callback for PNG overlay compositing.
// Transparent pixels (alpha < 128) are skipped so the reader page shows through.
// Opaque pixels are drawn in their grayscale brightness (dark → black, light → white).
int pngOverlayDraw(PNGDRAW* pDraw) {
  PngOverlayCtx* ctx = reinterpret_cast<PngOverlayCtx*>(pDraw->pUser);

  // Lazy-init: tRNS chunk is processed during decode() before any IDAT data, so by the time
  // the first draw callback fires, hasAlpha() / getTransparentColor() are already valid.
  if (ctx->transparentColor == -2) {
    const int pt = pDraw->iPixelType;
    ctx->transparentColor = (pDraw->iHasAlpha && (pt == PNG_PIXEL_TRUECOLOR || pt == PNG_PIXEL_GRAYSCALE))
                                ? static_cast<int32_t>(ctx->pngObj->getTransparentColor())
                                : -1;
  }

  const int destY = ctx->dstY + (int)(pDraw->y * ctx->yScale);
  if (destY == ctx->lastDstY) return 1;  // skip duplicate rows from Y scaling
  ctx->lastDstY = destY;
  if (destY < 0 || destY >= ctx->screenH) return 1;

  const int srcWidth = ctx->srcWidth;
  const int dstWidth = ctx->dstWidth;
  const uint8_t* pixels = pDraw->pPixels;
  const int pixelType = pDraw->iPixelType;
  const int hasAlpha = pDraw->iHasAlpha;

  int srcX = 0, error = 0;
  for (int dstX = 0; dstX < dstWidth; dstX++) {
    const int outX = ctx->dstX + dstX;
    if (outX >= 0 && outX < ctx->screenW) {
      uint8_t alpha = 255, gray = 0;
      switch (pixelType) {
        case PNG_PIXEL_TRUECOLOR_ALPHA: {
          const uint8_t* p = &pixels[srcX * 4];
          alpha = p[3];
          gray = (uint8_t)((p[0] * 77 + p[1] * 150 + p[2] * 29) >> 8);
          break;
        }
        case PNG_PIXEL_GRAY_ALPHA:
          gray = pixels[srcX * 2];
          alpha = pixels[srcX * 2 + 1];
          break;
        case PNG_PIXEL_TRUECOLOR: {
          const uint8_t* p = &pixels[srcX * 3];
          gray = (uint8_t)((p[0] * 77 + p[1] * 150 + p[2] * 29) >> 8);
          // tRNS color-key: if pixel matches the designated transparent color, skip it
          if (ctx->transparentColor >= 0 && p[0] == (uint8_t)((ctx->transparentColor >> 16) & 0xFF) &&
              p[1] == (uint8_t)((ctx->transparentColor >> 8) & 0xFF) &&
              p[2] == (uint8_t)(ctx->transparentColor & 0xFF)) {
            alpha = 0;
          }
          break;
        }
        case PNG_PIXEL_GRAYSCALE:
          gray = pixels[srcX];
          // tRNS color-key: transparent gray value stored in low byte
          if (ctx->transparentColor >= 0 && gray == (uint8_t)(ctx->transparentColor & 0xFF)) {
            alpha = 0;
          }
          break;
        case PNG_PIXEL_INDEXED:
          if (pDraw->pPalette) {
            const uint8_t idx = pixels[srcX];
            const uint8_t* p = &pDraw->pPalette[idx * 3];
            gray = (uint8_t)((p[0] * 77 + p[1] * 150 + p[2] * 29) >> 8);
            if (hasAlpha) alpha = pDraw->pPalette[768 + idx];
          }
          break;
        default:
          gray = pixels[srcX];
          break;
      }

      if (alpha >= 128) {
        ctx->renderer->drawPixel(outX, destY, gray < 128);  // true = black, false = white
      }
      // alpha < 128: transparent — leave the reader page pixel intact
    }

    // Bresenham-style X stepping (handles downscaling; 1:1 when srcWidth == dstWidth)
    error += srcWidth;
    while (error >= dstWidth) {
      error -= dstWidth;
      srcX++;
    }
  }
  return 1;
}

std::string filenameFromPath(const std::string& path) {
  const size_t lastSlash = path.find_last_of('/');
  return lastSlash == std::string::npos ? path : path.substr(lastSlash + 1);
}

std::string recentTitleForPath(const std::string& path) {
  const auto& books = RECENT_BOOKS.getBooks();
  const auto book = std::find_if(books.begin(), books.end(), [&path](const RecentBook& candidate) {
    return candidate.path == path && !candidate.title.empty();
  });
  return book == books.end() ? std::string{} : book->title;
}

RecentBook recentBookForPath(const std::string& path) {
  const auto& books = RECENT_BOOKS.getBooks();
  const auto book =
      std::find_if(books.begin(), books.end(), [&path](const RecentBook& candidate) { return candidate.path == path; });
  if (book != books.end()) {
    return *book;
  }

  RecentBook loadedBook = RECENT_BOOKS.getDataFromBook(path);
  if (loadedBook.title.empty()) {
    loadedBook.title = filenameFromPath(path);
  }
  return loadedBook;
}

std::string bookStatsCachePathFor(const std::string& path) {
  if (FsHelpers::hasEpubExtension(path)) {
    return Epub::cachePathForFilePath(path, "/.crosspoint");
  }
  if (FsHelpers::hasXtcExtension(path)) {
    return Xtc(path, "/.crosspoint").getCachePath();
  }
  return {};
}

BookReadingStats loadBookStatsForPath(const std::string& path) {
  const std::string cachePath = bookStatsCachePathFor(path);
  if (cachePath.empty()) {
    return BookReadingStats{};
  }
  return BookReadingStats::load(cachePath);
}

// Draws a book cover BMP scaled to fit within r (aspect preserved, centred). Returns false when
// the file is missing or cannot be parsed, so callers can try another path or a placeholder.
bool tryDrawCoverBmp(const GfxRenderer& renderer, const std::string& bmpPath, const Rect& r) {
  if (bmpPath.empty() || !Storage.exists(bmpPath.c_str())) {
    return false;
  }
  FsFile file;
  if (!Storage.openFileForRead("SLP", bmpPath, file)) {
    return false;
  }
  bool drawn = false;
  Bitmap bitmap(file);
  if (bitmap.parseHeaders() == BmpReaderError::Ok && bitmap.getWidth() > 0 && bitmap.getHeight() > 0) {
    const float scale = std::min(static_cast<float>(r.width) / static_cast<float>(bitmap.getWidth()),
                                 static_cast<float>(r.height) / static_cast<float>(bitmap.getHeight()));
    const int w = std::max(1, static_cast<int>(bitmap.getWidth() * scale));
    const int h = std::max(1, static_cast<int>(bitmap.getHeight() * scale));
    const int x = r.x + (r.width - w) / 2;
    const int y = r.y + (r.height - h) / 2;
    renderer.fillRoundedRect(x, y, w, h, 4, Color::White);
    renderer.drawBitmap(bitmap, x, y, w, h);
    renderer.drawRoundedRect(x, y, w, h, 1, 4, true);
    drawn = true;
  }
  file.close();
  return drawn;
}

// Draws the most-recently-read book's cover into r. For EPUBs, generates (or reuses) an
// adaptive-fit thumbnail sized to r itself (not a fixed Dashboard-sized one) — the JPEG/PNG
// converter prescales to its target dimensions before dithering specifically to avoid dithering
// artifacts from a later, separate downscale (see JpegToBmpConverter's USE_PRESCALE comment).
// Generating at a fixed 296x444 and then letting tryDrawCoverBmp's nearest-neighbour scale-fit
// shrink that down to a small card box re-samples already-dithered pixels, which is what made the
// small sleep-screen cover render mostly black. Requesting the real target size here instead lets
// the converter dither once at (close to) final resolution, and generateAdaptiveThumbBmp
// short-circuits instantly when a matching-size file is already cached, so calling it on every
// render is still cheap. Non-EPUB formats fall back to whatever cover thumb the recent-books store
// already has cached. Falls back to an empty framed box only if no source cover exists at all
// (e.g. the book file was removed).
void drawLastBookCover(const GfxRenderer& renderer, const RecentBook& book, const Rect& r) {
  std::string bmpPath;
  if (FsHelpers::hasEpubExtension(book.path)) {
    Epub epub(book.path, "/.crosspoint");
    bmpPath = epub.getAdaptiveThumbBmpPath(r.width, r.height);
    if (!Storage.exists(bmpPath.c_str()) && epub.load(/*buildIfMissing=*/true, /*skipLoadingCss=*/true)) {
      epub.generateAdaptiveThumbBmp(r.width, r.height, &renderer, SETTINGS.getReaderFontId());
    }
  } else {
    bmpPath = UITheme::getCoverThumbPath(book.coverBmpPath, r.width, r.height);
  }

  if (tryDrawCoverBmp(renderer, bmpPath, r)) {
    return;
  }
  renderer.drawRoundedRect(r.x, r.y, r.width, r.height, 1, 4, true);
}

// Fills leftover space below the plant-sleep-screen footer with aggregate reading stats plus a
// small cover of the last book read on the left.
void drawSleepReadingStatsCard(const GfxRenderer& renderer, const Rect& card, const RecentBook* lastBook,
                               const GlobalReadingStats& stats, const PetFarmState& farm) {
  renderer.drawRoundedRect(card.x, card.y, card.width, card.height, 1, 8, true);
  const int dividerY = card.y + 24;
  renderer.drawLine(card.x, dividerY, card.x + card.width - 1, dividerY, true);
  const char* title = "READING STATS";
  const int titleW = renderer.getTextWidth(UI_10_FONT_ID, title, EpdFontFamily::BOLD);
  renderer.drawText(UI_10_FONT_ID, card.x + (card.width - titleW) / 2, card.y + 5, title, true, EpdFontFamily::BOLD);

  // Give the cover as much of the card's height as possible; pad kept tight so it reads as a
  // real book cover rather than a small icon.
  const int pad = 10;
  const int coverTop = dividerY + 6;
  const int coverH = card.y + card.height - coverTop - pad;
  const int coverW = std::max(1, coverH * 2 / 3);
  if (lastBook != nullptr) {
    drawLastBookCover(renderer, *lastBook, Rect{card.x + pad, coverTop, coverW, coverH});
  }

  const int textX = card.x + pad + coverW + 18;
  const int rightEdge = card.x + card.width - pad;
  const bool twoCols = (rightEdge - textX) >= 300;
  const int col2X = twoCols ? textX + (rightEdge - textX) / 2 : textX;

  char timeBuf[40];
  BookReadingStats::formatDuration(stats.totalReadingSeconds, timeBuf, sizeof(timeBuf));

  char v0[40];
  char v1[16];
  char v2[16];
  char v3[24];
  snprintf(v0, sizeof(v0), "%s", timeBuf);
  snprintf(v1, sizeof(v1), "%lu", static_cast<unsigned long>(stats.completedBooks));
  snprintf(v2, sizeof(v2), "%lu", static_cast<unsigned long>(stats.totalPagesTurned));
  snprintf(v3, sizeof(v3), "%u days", static_cast<unsigned>(farm.currentStreak));

  struct StatItem {
    const char* label;
    const char* value;
  };
  const StatItem items[4] = {
    {"TIME READ", v0},
    {"BOOKS READ", v1},
    {"PAGES READ", v2},
    {"STREAK", v3},
  };

  // Bold caps label above a larger regular-weight value, matching the bold-label/plain-value
  // stat-tile language used elsewhere on this screen. Rows are spread evenly across the full
  // cover height (via the same statsBlockTop helper the plant vitals column uses) instead of a
  // fixed tight rowH, so the block fills the card instead of leaving empty space below it.
  const int labelLineH = renderer.getLineHeight(SMALL_FONT_ID);
  const int valueLineH = renderer.getLineHeight(UI_10_FONT_ID);
  constexpr int kLabelValueGap = 4;
  const int blockH = labelLineH + kLabelValueGap + valueLineH;
  const int rowCount = twoCols ? 2 : 4;
  const Rect statsSpan{textX, coverTop, rightEdge - textX, coverH};

  auto drawStatItem = [&](const int x, const int y, const StatItem& item) {
    renderer.drawText(SMALL_FONT_ID, x, y, item.label, true, EpdFontFamily::BOLD);
    renderer.drawText(UI_10_FONT_ID, x, y + labelLineH + kLabelValueGap, item.value);
  };

  for (int row = 0; row < rowCount; row++) {
    const int rowY = statsBlockTop(statsSpan, row, blockH, rowCount);
    if (twoCols) {
      drawStatItem(textX, rowY, items[row * 2]);
      drawStatItem(col2X, rowY, items[row * 2 + 1]);
    } else {
      drawStatItem(textX, rowY, items[row]);
    }
  }
}

std::string loadChapterTitleForPath(const std::string& path) {
  if (!FsHelpers::hasEpubExtension(path)) {
    return {};
  }

  Epub epub(path, "/.crosspoint");
  if (!epub.load(false, true)) {
    return {};
  }

  EpubReaderUtils::Progress progress;
  if (!EpubReaderUtils::loadProgress(epub, progress, "SLP")) {
    return {};
  }

  const auto spineItem = epub.getSpineItem(progress.spineIndex);
  if (spineItem.tocIndex < 0) {
    return {};
  }

  const auto tocItem = epub.getTocItem(spineItem.tocIndex);
  return tocItem.title;
}

enum class OverlayDrawResult : uint8_t { NotFound, Drawn, Failed };

enum class SleepImageMode : uint8_t { Custom, Overlay };

struct SleepImageSelection {
  std::string path;
  bool isPng = false;
};

bool isBmpSleepImagePath(const std::string& path) { return FsHelpers::hasBmpExtension(path); }

bool isPngSleepImagePath(const std::string& path) { return FsHelpers::hasPngExtension(path); }

bool tryOpenSleepDirectory(FsFile& dir, std::string& sleepDir, const std::string& candidate) {
  if (candidate.empty()) {
    return false;
  }

  dir = Storage.open(candidate.c_str());
  if (dir && dir.isDirectory()) {
    sleepDir = candidate;
    return true;
  }

  if (dir) {
    dir.close();
  }
  return false;
}

bool openPreferredSleepDirectory(FsFile& dir, std::string& sleepDir) {
  sleepDir.clear();

  if (tryOpenSleepDirectory(dir, sleepDir, APP_STATE.preferredSleepFolderPath)) {
    return true;
  }

  if (!APP_STATE.preferredSleepFolderPath.empty()) {
    LOG_INF("SLP", "Preferred sleep folder missing, falling back: %s", APP_STATE.preferredSleepFolderPath.c_str());
  }

  if (tryOpenSleepDirectory(dir, sleepDir, "/.sleep")) {
    return true;
  }

  return tryOpenSleepDirectory(dir, sleepDir, "/sleep");
}

bool selectPinnedSleepImage(SleepImageMode mode, SleepImageSelection& selection) {
  const std::string& favorite = APP_STATE.favoriteSleepImagePath;
  if (favorite.empty()) {
    return false;
  }

  if (!Storage.exists(favorite.c_str())) {
    LOG_INF("SLP", "Pinned sleep image missing, falling back: %s", favorite.c_str());
    return false;
  }

  if (isBmpSleepImagePath(favorite)) {
    selection.path = favorite;
    selection.isPng = false;
    return true;
  }

  if (isPngSleepImagePath(favorite)) {
    if (mode == SleepImageMode::Overlay) {
      selection.path = favorite;
      selection.isPng = true;
      return true;
    }

    LOG_INF("SLP", "Pinned PNG sleep image requires Page Overlay mode, falling back: %s", favorite.c_str());
    return false;
  }

  LOG_ERR("SLP", "Pinned sleep image has unsupported extension: %s", favorite.c_str());
  return false;
}

bool selectRandomSleepImage(SleepImageMode mode, SleepImageSelection& selection) {
  FsFile dir;
  std::string sleepDir;
  if (!openPreferredSleepDirectory(dir, sleepDir)) {
    return false;
  }

  const bool allowPng = mode == SleepImageMode::Overlay;
  std::vector<std::string> files;
  files.reserve(16);
  char name[500];
  for (auto file = dir.openNextFile(); file; file = dir.openNextFile()) {
    if (file.isDirectory()) {
      file.close();
      continue;
    }

    file.getName(name, sizeof(name));
    std::string filename(name);
    if (filename.empty() || filename[0] == '.') {
      file.close();
      continue;
    }

    const bool isBmp = FsHelpers::hasBmpExtension(filename);
    const bool isPng = allowPng && FsHelpers::hasPngExtension(filename);
    if (!isBmp && !isPng) {
      file.close();
      continue;
    }

    if (isBmp) {
      Bitmap bitmap(file);
      const BmpReaderError parseResult = bitmap.parseHeaders();
      if (parseResult != BmpReaderError::Ok) {
        LOG_ERR("SLP", "Skipping invalid BMP sleep image %s/%s: %s", sleepDir.c_str(), filename.c_str(),
                Bitmap::errorToString(parseResult));
        file.close();
        continue;
      }
    }

    files.emplace_back(std::move(filename));
    file.close();
  }
  dir.close();

  if (files.empty()) {
    return false;
  }

  const uint16_t fileCount = static_cast<uint16_t>(std::min(files.size(), static_cast<size_t>(UINT16_MAX)));
  const uint8_t window =
      static_cast<uint8_t>(std::min(static_cast<size_t>(APP_STATE.recentSleepFill), files.size() - 1));
  auto randomFileIndex = static_cast<uint16_t>(random(fileCount));
  for (uint8_t attempt = 0; attempt < 20 && APP_STATE.isRecentSleep(randomFileIndex, window); attempt++) {
    randomFileIndex = static_cast<uint16_t>(random(fileCount));
  }

  APP_STATE.pushRecentSleep(randomFileIndex);
  APP_STATE.saveToFile();
  selection.path = sleepDir + "/" + files[randomFileIndex];
  selection.isPng = FsHelpers::hasPngExtension(selection.path);
  return true;
}

}  // namespace

void SleepActivity::onEnter() {
  Activity::onEnter();

  const bool renderQuickResume =
      SETTINGS.sleepScreen == CrossPointSettings::SLEEP_SCREEN_MODE::QUICK_RESUME ||
      (fromTimeout &&
       SETTINGS.quickResumeSleepScreen == CrossPointSettings::QUICK_RESUME_SLEEP_SCREEN::QUICK_RESUME_AFTER_TIMEOUT);

  if (renderQuickResume) {
    return renderLastScreenSleepScreen();
  }

  overlayBackgroundBufferStored =
      SETTINGS.sleepScreen == CrossPointSettings::SLEEP_SCREEN_MODE::OVERLAY && renderer.storeBwBuffer();

  // Show the popup in the reader's orientation when sleep starts from an open book.
  // Reset to portrait afterwards so the sleep screen renderer keeps its existing layout.
  if (APP_STATE.lastSleepFromReader) {
    ReaderUtils::applyOrientation(renderer, SETTINGS.orientation);
    GUI.drawPopup(renderer, tr(STR_ENTERING_SLEEP));
    renderer.setOrientation(GfxRenderer::Orientation::Portrait);
  } else {
    GUI.drawPopup(renderer, tr(STR_ENTERING_SLEEP));
  }

  switch (SETTINGS.sleepScreen) {
    case (CrossPointSettings::SLEEP_SCREEN_MODE::BLANK):
      return renderBlankSleepScreen();
    case (CrossPointSettings::SLEEP_SCREEN_MODE::CUSTOM):
      return renderCustomSleepScreen();
    case (CrossPointSettings::SLEEP_SCREEN_MODE::COVER):
      return renderCoverSleepScreen();
    case (CrossPointSettings::SLEEP_SCREEN_MODE::COVER_CUSTOM):
      if (APP_STATE.lastSleepFromReader) {
        return renderCoverSleepScreen();
      } else {
        return renderCustomSleepScreen();
      }
    case (CrossPointSettings::SLEEP_SCREEN_MODE::OVERLAY):
      return renderOverlaySleepScreen();
    case (CrossPointSettings::SLEEP_SCREEN_MODE::READING_STATS_SLEEP):
      return renderReadingStatsSleepScreen();
    case (CrossPointSettings::SLEEP_SCREEN_MODE::MINIMAL_SLEEP):
      return renderMinimalSleepScreen();
    case (CrossPointSettings::SLEEP_SCREEN_MODE::MINIMAL_STATS_SLEEP):
      return renderMinimalStatsSleepScreen();
    case (CrossPointSettings::SLEEP_SCREEN_MODE::DASHBOARD_SLEEP):
      return renderDashboardSleepScreen();
    case (CrossPointSettings::SLEEP_SCREEN_MODE::PET_SLEEP):
      return renderPetSleepScreen();
    default:
      return renderDefaultSleepScreen();
  }
}

void SleepActivity::renderCustomSleepScreen() const {
  SleepImageSelection selection;
  if (selectPinnedSleepImage(SleepImageMode::Custom, selection) ||
      selectRandomSleepImage(SleepImageMode::Custom, selection)) {
    FsFile file;
    if (Storage.openFileForRead("SLP", selection.path, file)) {
      LOG_INF("SLP", "Loading custom sleep image: %s", selection.path.c_str());
      delay(100);
      Bitmap bitmap(file, true);
      if (bitmap.parseHeaders() == BmpReaderError::Ok) {
        renderBitmapSleepScreen(bitmap);
        return;
      }
      LOG_ERR("SLP", "Failed to parse custom sleep BMP: %s", selection.path.c_str());
    } else {
      LOG_ERR("SLP", "Failed to open custom sleep image: %s", selection.path.c_str());
    }
  }

  // Look for sleep.bmp on the root of the sd card to determine if we should
  // render a custom sleep screen instead of the default.
  FsFile file;
  if (Storage.openFileForRead("SLP", "/sleep.bmp", file)) {
    Bitmap bitmap(file, true);
    if (bitmap.parseHeaders() == BmpReaderError::Ok) {
      LOG_DBG("SLP", "Loading: /sleep.bmp");
      renderBitmapSleepScreen(bitmap);
      return;
    }
  }

  renderDefaultSleepScreen();
}

void SleepActivity::renderDefaultSleepScreen() const {
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  renderer.drawImage(Seed, (pageWidth - 144) / 2, (pageHeight - 144) / 2, 144, 144);
  renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 82, tr(STR_CROSSINK), true, EpdFontFamily::BOLD);
  renderer.drawCenteredText(SMALL_FONT_ID, pageHeight / 2 + 107, tr(STR_SLEEPING));

  // Make sleep screen dark unless light is selected in settings
  const bool lightSleepScreen = SETTINGS.sleepScreen == CrossPointSettings::SLEEP_SCREEN_MODE::LIGHT;
  if (!lightSleepScreen) {
    renderer.invertScreen();
  }

#ifdef CROSSINK_SHOW_SLEEP_BUILD_INFO
  const std::string buildInfo = std::string(CROSSINK_BUILD_ENV) + " " + CROSSINK_VERSION;
  const std::string visibleBuildInfo =
      renderer.truncatedText(SMALL_FONT_ID, buildInfo.c_str(), pageWidth - sleepBuildInfoSideMargin * 2);
  renderer.drawCenteredText(SMALL_FONT_ID, pageHeight / 2 + 130, visibleBuildInfo.c_str(), lightSleepScreen);
#endif

  renderer.displayBuffer(HalDisplay::FULL_REFRESH, TURN_OFF_SCREEN_AFTER_SLEEP_REFRESH);
}

void SleepActivity::renderPetSleepScreen() const {
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const bool isX3 = (pageWidth == 528);

  PET_MANAGER.load();
  const bool hasPet = PET_MANAGER.exists() && PET_MANAGER.isAlive();

  renderer.clearScreen();

  if (!hasPet) {
    renderer.drawImage(Seed, (pageWidth - 144) / 2, (pageHeight - 144) / 2, 144, 144);
    renderer.drawCenteredText(SMALL_FONT_ID, pageHeight / 2 + 82, tr(STR_SLEEPING));
    renderer.displayBuffer(HalDisplay::FULL_REFRESH, TURN_OFF_SCREEN_AFTER_SLEEP_REFRESH);
    return;
  }

  const auto& state = PET_MANAGER.getState();
  const auto& farm = PET_MANAGER.getFarmState();

  // 1. Draw Title Header
  renderer.drawCenteredText(UI_10_FONT_ID, 24, "CROSSPLANT DORMANT", true, EpdFontFamily::BOLD);
  renderer.drawLine(20, 48, pageWidth - 20, 48, true);

  // 2. Dashboard Layout Calculations
  const bool isWide = (pageWidth >= 560);
  const int inset = isWide ? 75 : 20;
  const int statsW = isWide ? 120 : 105;
  constexpr int kCoverStatsGap = 15;
  const int shift = isX3 ? 15 : 0;
  
  const int maxCoverW = pageWidth - inset * 2 - statsW - kCoverStatsGap;
  const int coverW = std::min(296, maxCoverW);
  const int maxCoverH = pageHeight - 70 - 100; // 70px top header, 100px bottom footer
  const int coverH = std::min(maxCoverH, (coverW * 3) / 2);
  
  const Rect coverRect{
    inset + shift,
    70,
    coverW,
    coverH
  };

  // 3. Draw Left Card Frame
  renderer.fillRoundedRect(coverRect.x, coverRect.y, coverRect.width, coverRect.height, 8, Color::White);
  renderer.drawRoundedRect(coverRect.x, coverRect.y, coverRect.width, coverRect.height, 1, 8, true);

  // 4. Draw Pet Sprite & Information inside Left Card
  // Scale the plant up to fill more of the card (was a fixed 144px); bounded by both card
  // dimensions so there's still room below it for the name/stage/species lines and card margins.
  constexpr int kMaxPetSize = 220;
  const int petSize = std::min({kMaxPetSize, coverRect.width - 24, coverRect.height - 110});
  const int spriteX = coverRect.x + (coverRect.width - petSize) / 2;
  const int spriteY = coverRect.y + (coverRect.height - petSize) / 2 - 20;

  PetSpriteRenderer::drawPet(renderer, spriteX, spriteY, state.stage, PetMood::SLEEPING, /*scale=*/4,
                             state.evolutionVariant, state.petType, /*animFrame=*/0, /*forceHat=*/false,
                             /*forceGlasses=*/false, /*targetSize=*/petSize);

  const char* petName = state.petName[0] ? state.petName : PetTypeNames::get(state.petType);
  const char* stageName = PetEvolution::variantStageName(state.stage, state.evolutionVariant, state.petType);
  const char* speciesName = PetTypeNames::get(state.petType);

  const int nameY = spriteY + petSize + 12;
  const int lineH10 = renderer.getLineHeight(UI_10_FONT_ID);
  const int lineH8 = renderer.getLineHeight(SMALL_FONT_ID);
  const int nameW = renderer.getTextWidth(UI_10_FONT_ID, petName, EpdFontFamily::BOLD);
  renderer.drawText(UI_10_FONT_ID, coverRect.x + (coverRect.width - nameW) / 2, nameY, petName, true, EpdFontFamily::BOLD);

  const int stageW = renderer.getTextWidth(SMALL_FONT_ID, stageName);
  renderer.drawText(SMALL_FONT_ID, coverRect.x + (coverRect.width - stageW) / 2, nameY + lineH10 + 2, stageName, true);

  const int speciesW = renderer.getTextWidth(SMALL_FONT_ID, speciesName);
  renderer.drawText(SMALL_FONT_ID, coverRect.x + (coverRect.width - speciesW) / 2,
                    nameY + lineH10 + 2 + lineH8 + 2, speciesName, true);

  // 5. Draw Right Column Stats
  const int rightX = pageWidth - inset - shift;
  const int blockH = statsBlockHeight(renderer);
  const int rowCount = 6;
  int rowIndex = 0;

  // Row 0: Age
  int rowY = statsBlockTop(coverRect, rowIndex, blockH, rowCount);
  char ageVal[32];
  snprintf(ageVal, sizeof(ageVal), "Day %lu", (unsigned long)(PET_MANAGER.getDaysAlive() + 1));
  drawStatsRow(renderer, rightX, rowY, ageVal, "PLANT AGE", true);

  // Row 1: Moisture
  rowY = statsBlockTop(coverRect, ++rowIndex, blockH, rowCount);
  char moistureVal[32];
  snprintf(moistureVal, sizeof(moistureVal), "%u%%", state.hunger);
  drawStatsRow(renderer, rightX, rowY, moistureVal, "SOIL MOISTURE", true);

  // Row 2: Sunlight
  rowY = statsBlockTop(coverRect, ++rowIndex, blockH, rowCount);
  char sunlightVal[32];
  snprintf(sunlightVal, sizeof(sunlightVal), "%u%%", state.happiness);
  drawStatsRow(renderer, rightX, rowY, sunlightVal, "SUN EXPOSURE", true);

  // Row 3: Nutrients
  rowY = statsBlockTop(coverRect, ++rowIndex, blockH, rowCount);
  char nutrientsVal[32];
  snprintf(nutrientsVal, sizeof(nutrientsVal), "%u%%", state.discipline);
  drawStatsRow(renderer, rightX, rowY, nutrientsVal, "NUTRIENTS", true);

  // Row 4: Health
  rowY = statsBlockTop(coverRect, ++rowIndex, blockH, rowCount);
  char healthVal[32];
  snprintf(healthVal, sizeof(healthVal), "%u%%", state.health);
  drawStatsRow(renderer, rightX, rowY, healthVal, "PLANT HEALTH", true);

  // Row 5: Slept time
  rowY = statsBlockTop(coverRect, ++rowIndex, blockH, rowCount);
  char sleepVal[48] = "Slept soundly";
  uint16_t year;
  uint8_t month, day, hour, minute;
  if (halClock.isAvailable() && halClock.getDateTime(year, month, day, hour, minute)) {
    // getDateTime() returns the raw UTC RTC time (see HalClock.h); apply the user's configured
    // offset the same way ReadingStatsUtils/ClippingsManager do, or this always shows UTC instead
    // of the wall-clock time the "REST TIME" label promises.
    const int offsetMinutes = (static_cast<int>(std::min<uint8_t>(SETTINGS.clockUtcOffsetQ, 104)) - 48) * 15;
    int totalMinutes = static_cast<int>(hour) * 60 + static_cast<int>(minute) + offsetMinutes;
    totalMinutes = ((totalMinutes % (24 * 60)) + 24 * 60) % (24 * 60);
    int h = totalMinutes / 60;
    const int m = totalMinutes % 60;
    const char* ampm = (h >= 12) ? "PM" : "AM";
    if (h > 12) h -= 12;
    if (h == 0) h = 12;
    snprintf(sleepVal, sizeof(sleepVal), "%d:%02d %s", h, m, ampm);
  }
  drawStatsRow(renderer, rightX, rowY, sleepVal, "REST TIME", true);

  // 6. Draw Bottom Footer
  std::string boostsStr = "";
  if (farm.hasMossPole && farm.equipMossPole) boostsStr += "Moss Pole, ";
  if (farm.hasSelfWateringPot && farm.equipSelfWateringPot) boostsStr += "Watering Pot, ";
  if (farm.hasSlowReleaseFertilizer && farm.equipSlowReleaseFertilizer) boostsStr += "Slow-Fert, ";
  if (farm.hasGreenhouseCover && farm.equipGreenhouseCover) boostsStr += "Greenhouse, ";
  if (farm.hasPremiumSprayer) boostsStr += "Sprayer, ";
  if (!boostsStr.empty()) {
    boostsStr = boostsStr.substr(0, boostsStr.length() - 2);
  } else {
    boostsStr = "None";
  }

  const char* wLabel = "Offline";
  const char* wBonus = "None";
  if (farm.weatherCondition == 1) { wLabel = "Sunny"; wBonus = "Light (+1 Sun/h)"; }
  else if (farm.weatherCondition == 2) { wLabel = "Rainy"; wBonus = "Humidity (+1 Moist/h)"; }
  else if (farm.weatherCondition == 3) { wLabel = "Cloudy"; wBonus = "Nutrient (+1 Nut/h)"; }
  else if (farm.weatherCondition == 4) { wLabel = "Snowy"; wBonus = "Greenhouse (+1 HP/h)"; }

  char weatherLine[128];
  snprintf(weatherLine, sizeof(weatherLine), "Weather: %s (%s)", wLabel, wBonus);

  char balanceLine[64];
  snprintf(balanceLine, sizeof(balanceLine), "Balance: %lu $Dew", (unsigned long)farm.inkPoints);

  const int footerY = coverRect.y + coverRect.height + 25;
  const int lineH = renderer.getLineHeight(SMALL_FONT_ID);
  
  renderer.drawLine(inset, footerY - 10, pageWidth - inset, footerY - 10, true);
  
  // Footer Line 1: Left active boosts, Right balance
  char boostsLine[128];
  snprintf(boostsLine, sizeof(boostsLine), "Active Boosts: %s", boostsStr.c_str());
  renderer.drawText(SMALL_FONT_ID, inset, footerY, boostsLine);
  drawRightAlignedText(renderer, SMALL_FONT_ID, rightX, footerY, balanceLine, false, true);
  
  // Footer Line 2: Left weather line, Right sleep status
  renderer.drawText(SMALL_FONT_ID, inset, footerY + lineH + 6, weatherLine);
  drawRightAlignedText(renderer, SMALL_FONT_ID, rightX, footerY + lineH + 6, "CrossPlant Dormant", false, true);

  // 7. Reading stats + last-book cover, filling any leftover space below the boosts/weather
  // footer (mainly present on the taller X3 panel).
  constexpr int kStatsCardTopGap = 14;
  constexpr int kBottomMargin = 16;
  // Below this, the cover would be too small to read as a book cover; skip the card entirely
  // rather than draw a cramped sliver (mainly relevant on the shorter X4 panel).
  constexpr int kMinStatsCardHeight = 140;
  const int statsCardTop = footerY + lineH + 6 + lineH + kStatsCardTopGap;
  const int statsCardHeight = pageHeight - kBottomMargin - statsCardTop;
  if (statsCardHeight >= kMinStatsCardHeight) {
    const GlobalReadingStats globalStats = GlobalReadingStats::load();
    const auto& books = RECENT_BOOKS.getBooks();
    const RecentBook* lastBook = books.empty() ? nullptr : &books.front();
    const Rect statsCard{coverRect.x, statsCardTop, rightX - coverRect.x, statsCardHeight};
    drawSleepReadingStatsCard(renderer, statsCard, lastBook, globalStats, farm);
  }

  renderer.displayBuffer(HalDisplay::FULL_REFRESH, TURN_OFF_SCREEN_AFTER_SLEEP_REFRESH);
}

void SleepActivity::renderBitmapSleepScreen(const Bitmap& bitmap) const {
  int x, y;
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  float cropX = 0, cropY = 0;

  LOG_DBG("SLP", "bitmap %d x %d, screen %d x %d", bitmap.getWidth(), bitmap.getHeight(), pageWidth, pageHeight);
  if (bitmap.getWidth() > pageWidth || bitmap.getHeight() > pageHeight) {
    // image will scale, make sure placement is right
    float ratio = static_cast<float>(bitmap.getWidth()) / static_cast<float>(bitmap.getHeight());
    const float screenRatio = static_cast<float>(pageWidth) / static_cast<float>(pageHeight);

    LOG_DBG("SLP", "bitmap ratio: %f, screen ratio: %f", ratio, screenRatio);
    if (ratio > screenRatio) {
      // image wider than viewport ratio, scaled down image needs to be centered vertically
      if (SETTINGS.sleepScreenCoverMode == CrossPointSettings::SLEEP_SCREEN_COVER_MODE::CROP) {
        cropX = 1.0f - (screenRatio / ratio);
        LOG_DBG("SLP", "Cropping bitmap x: %f", cropX);
        ratio = (1.0f - cropX) * static_cast<float>(bitmap.getWidth()) / static_cast<float>(bitmap.getHeight());
      }
      x = 0;
      y = std::round((static_cast<float>(pageHeight) - static_cast<float>(pageWidth) / ratio) / 2);
      LOG_DBG("SLP", "Centering with ratio %f to y=%d", ratio, y);
    } else {
      // image taller than viewport ratio, scaled down image needs to be centered horizontally
      if (SETTINGS.sleepScreenCoverMode == CrossPointSettings::SLEEP_SCREEN_COVER_MODE::CROP) {
        cropY = 1.0f - (ratio / screenRatio);
        LOG_DBG("SLP", "Cropping bitmap y: %f", cropY);
        ratio = static_cast<float>(bitmap.getWidth()) / ((1.0f - cropY) * static_cast<float>(bitmap.getHeight()));
      }
      x = std::round((static_cast<float>(pageWidth) - static_cast<float>(pageHeight) * ratio) / 2);
      y = 0;
      LOG_DBG("SLP", "Centering with ratio %f to x=%d", ratio, x);
    }
  } else {
    // center the image
    x = (pageWidth - bitmap.getWidth()) / 2;
    y = (pageHeight - bitmap.getHeight()) / 2;
  }

  LOG_DBG("SLP", "drawing to %d x %d", x, y);
  renderer.clearScreen();

  const bool hasGreyscale = bitmap.hasGreyscale() &&
                            SETTINGS.sleepScreenCoverFilter == CrossPointSettings::SLEEP_SCREEN_COVER_FILTER::NO_FILTER;

  renderer.drawBitmap(bitmap, x, y, pageWidth, pageHeight, cropX, cropY);

  if (SETTINGS.sleepScreenCoverFilter == CrossPointSettings::SLEEP_SCREEN_COVER_FILTER::INVERTED_BLACK_AND_WHITE) {
    renderer.invertScreen();
  }

  if (hasGreyscale) {
    // OEM grayscale pipeline base: use a full sleep-screen paint so the panel
    // enters deep sleep from a clean B/W baseline before the gray nudge refresh.
    renderer.displayGrayscaleBase(HalDisplay::FULL_REFRESH);
  } else {
    renderer.displayBuffer(HalDisplay::FULL_REFRESH, TURN_OFF_SCREEN_AFTER_SLEEP_REFRESH);
  }

  if (hasGreyscale) {
    bitmap.rewindToData();
    renderer.clearScreen(0x00);
    renderer.setRenderMode(GfxRenderer::GRAYSCALE_LSB);
    renderer.drawBitmap(bitmap, x, y, pageWidth, pageHeight, cropX, cropY);
    renderer.copyGrayscaleLsbBuffers();

    bitmap.rewindToData();
    renderer.clearScreen(0x00);
    renderer.setRenderMode(GfxRenderer::GRAYSCALE_MSB);
    renderer.drawBitmap(bitmap, x, y, pageWidth, pageHeight, cropX, cropY);
    renderer.copyGrayscaleMsbBuffers();

    renderer.displayGrayBuffer(TURN_OFF_SCREEN_AFTER_SLEEP_REFRESH);
    renderer.setRenderMode(GfxRenderer::BW);
  }
}

void SleepActivity::renderCoverSleepScreen() const {
  void (SleepActivity::*renderNoCoverSleepScreen)() const;
  switch (SETTINGS.sleepScreen) {
    case (CrossPointSettings::SLEEP_SCREEN_MODE::COVER_CUSTOM):
      renderNoCoverSleepScreen = &SleepActivity::renderCustomSleepScreen;
      break;
    default:
      renderNoCoverSleepScreen = &SleepActivity::renderDefaultSleepScreen;
      break;
  }

  const std::string& path = currentBookPath.empty() ? APP_STATE.openEpubPath : currentBookPath;
  if (path.empty()) {
    return (this->*renderNoCoverSleepScreen)();
  }

  bool cropped = SETTINGS.sleepScreenCoverMode == CrossPointSettings::SLEEP_SCREEN_COVER_MODE::CROP;
  std::string coverBmpPath = SleepCoverAssets::cachedCoverPathFor(path, cropped);
  if (coverBmpPath.empty() && SleepCoverAssets::prepareFullCoverForPath(path, cropped, &renderer)) {
    coverBmpPath = SleepCoverAssets::cachedCoverPathFor(path, cropped);
  }
  if (coverBmpPath.empty()) {
    return (this->*renderNoCoverSleepScreen)();
  }

  FsFile file;
  if (Storage.openFileForRead("SLP", coverBmpPath, file)) {
    Bitmap bitmap(file);
    if (bitmap.parseHeaders() == BmpReaderError::Ok) {
      LOG_DBG("SLP", "Rendering sleep cover: %s", coverBmpPath.c_str());
      renderBitmapSleepScreen(bitmap);
      return;
    }
  }

  return (this->*renderNoCoverSleepScreen)();
}

void SleepActivity::renderReadingStatsSleepScreen() const {
  BookReadingStats bookStats;
  std::string bookTitle = tr(STR_READING_STATS);
  float progressPercent = -1.0f;

  const std::string& path = currentBookPath.empty() ? APP_STATE.openEpubPath : currentBookPath;
  if (!path.empty()) {
    const std::string recentTitle = recentTitleForPath(path);
    bookTitle = recentTitle.empty() ? filenameFromPath(path) : recentTitle;

    bookStats = loadBookStatsForPath(path);
    progressPercent = RecentBookProgress::loadPercent(recentBookForPath(path));
  }

  if (!halClock.isAvailable()) {
    const GlobalReadingStats deviceStats = GlobalReadingStats::load();
    const bool hasSyncedStats = GlobalReadingStats::hasSyncedStats();
    const GlobalReadingStats allDevicesStats =
        hasSyncedStats ? GlobalReadingStats::loadAggregated(deviceStats) : GlobalReadingStats{};
    renderNoRtcCombinedStatsPage(renderer, nullptr, bookTitle, bookStats, progressPercent, false, 0, deviceStats,
                                 hasSyncedStats ? &allDevicesStats : nullptr, false);
  } else {
    renderPerBookStatsPage(renderer, nullptr, bookTitle, bookStats, progressPercent, false, 0, false, false, false);
  }
  if (!sleepCoverFilterInvertsGeneratedScreen()) {
    renderer.invertScreen();
  }
  renderer.displayBuffer(HalDisplay::FULL_REFRESH, TURN_OFF_SCREEN_AFTER_SLEEP_REFRESH);
}

void SleepActivity::renderMinimalSleepScreen() const {
  const std::string& path = currentBookPath.empty() ? APP_STATE.openEpubPath : currentBookPath;
  if (path.empty()) {
    return renderDefaultSleepScreen();
  }

  RecentBook book = recentBookForPath(path);
  book.coverBmpPath = SleepCoverAssets::cachedMinimalCoverPathFor(path);
  if (book.coverBmpPath.empty() && SleepCoverAssets::prepareMinimalCoverForPath(path, &renderer)) {
    book.coverBmpPath = SleepCoverAssets::cachedMinimalCoverPathFor(path);
  }

  const BookReadingStats bookStats = loadBookStatsForPath(path);
  const float progressPercent = RecentBookProgress::loadPercent(book);
  MinimalTheme theme;
  theme.drawSleepScreen(renderer, book, &bookStats, progressPercent, sleepCoverFilterInvertsGeneratedScreen());
  renderer.displayBuffer(HalDisplay::FULL_REFRESH, TURN_OFF_SCREEN_AFTER_SLEEP_REFRESH);
}

void SleepActivity::renderMinimalStatsSleepScreen() const {
  const std::string& path = currentBookPath.empty() ? APP_STATE.openEpubPath : currentBookPath;
  if (path.empty()) {
    return renderDefaultSleepScreen();
  }

  RecentBook book = recentBookForPath(path);
  book.coverBmpPath = SleepCoverAssets::cachedMinimalCoverPathFor(path);
  if (book.coverBmpPath.empty() && SleepCoverAssets::prepareMinimalCoverForPath(path, &renderer)) {
    book.coverBmpPath = SleepCoverAssets::cachedMinimalCoverPathFor(path);
  }

  const BookReadingStats bookStats = loadBookStatsForPath(path);
  const GlobalReadingStats globalStats = GlobalReadingStats::load();
  const float progressPercent = RecentBookProgress::loadPercent(book);
  MinimalTheme theme;
  theme.drawStatsSleepScreen(renderer, book, &bookStats, &globalStats, progressPercent,
                             sleepCoverFilterInvertsGeneratedScreen());
  renderer.displayBuffer(HalDisplay::FULL_REFRESH, TURN_OFF_SCREEN_AFTER_SLEEP_REFRESH);
}

void SleepActivity::renderDashboardSleepScreen() const {
  const std::string& path = currentBookPath.empty() ? APP_STATE.openEpubPath : currentBookPath;
  if (path.empty()) {
    return renderDefaultSleepScreen();
  }

  RecentBook book = recentBookForPath(path);
  const std::string fallbackCoverPath = book.coverBmpPath;
  book.coverBmpPath = SleepCoverAssets::cachedDashboardCoverPathFor(path);
  if (book.coverBmpPath.empty() && SleepCoverAssets::prepareDashboardCoverForPath(path, &renderer)) {
    book.coverBmpPath = SleepCoverAssets::cachedDashboardCoverPathFor(path);
  }
  if (book.coverBmpPath.empty()) {
    book.coverBmpPath = fallbackCoverPath;
  }

  const BookReadingStats bookStats = loadBookStatsForPath(path);
  const GlobalReadingStats globalStats = GlobalReadingStats::load();
  const float progressPercent = RecentBookProgress::loadPercent(book);
  const std::string chapterTitle = loadChapterTitleForPath(path);
  DashboardTheme theme;
  theme.drawSleepScreen(renderer, book, &bookStats, &globalStats, progressPercent, chapterTitle.c_str(),
                        sleepCoverFilterInvertsGeneratedScreen());
  renderer.displayBuffer(HalDisplay::FULL_REFRESH, TURN_OFF_SCREEN_AFTER_SLEEP_REFRESH);
}

void SleepActivity::renderLastScreenSleepScreen() const {
  const auto pageHeight = renderer.getScreenHeight();
  if (ReaderUtils::readerDarkModeEnabled()) {
    renderer.drawImageInverted(MoonIcon, 0, pageHeight - MOONICON_HEIGHT, MOONICON_WIDTH, MOONICON_HEIGHT);
  } else {
    renderer.drawImage(MoonIcon, 0, pageHeight - MOONICON_HEIGHT, MOONICON_WIDTH, MOONICON_HEIGHT);
  }
  renderer.displayBuffer(HalDisplay::FULL_REFRESH);
}

void SleepActivity::renderBlankSleepScreen() const {
  renderer.clearScreen();
  renderer.displayBuffer(HalDisplay::FULL_REFRESH, TURN_OFF_SCREEN_AFTER_SLEEP_REFRESH);
}

void SleepActivity::renderOverlaySleepScreen() const {
  // Overlay pictures always use portrait orientation regardless of the reader's orientation preference.
  const auto savedOrientation = renderer.getOrientation();
  renderer.setOrientation(GfxRenderer::Portrait);
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const bool shouldUseReaderPageBackground = canSnapshotOverlayBackground;
  const std::string path = shouldUseReaderPageBackground
                               ? (currentBookPath.empty() ? APP_STATE.openEpubPath : currentBookPath)
                               : std::string{};

  auto renderSavedReaderPage = [&]() -> bool {
    if (path.empty()) {
      return false;
    }

    if (FsHelpers::checkFileExtension(path, ".xtc") || FsHelpers::checkFileExtension(path, ".xtch")) {
      return XtcReaderActivity::drawCurrentPageToBuffer(path, renderer);
    }
    if (FsHelpers::checkFileExtension(path, ".txt")) {
      return TxtReaderActivity::drawCurrentPageToBuffer(path, renderer);
    }
    if (FsHelpers::checkFileExtension(path, ".epub")) {
      return EpubReaderActivity::drawCurrentPageToBuffer(path, renderer);
    }
    return false;
  };
  const bool backgroundSupportsGrayscale =
      FsHelpers::checkFileExtension(path, ".txt") || FsHelpers::checkFileExtension(path, ".epub");
  bool backgroundWasRebuilt = false;
  bool backgroundAvailable = false;

  // Step 1: Restore the screen that was visible before the sleep popup. When
  // that snapshot is unavailable in the reader, rebuild from the saved position.
  if (overlayBackgroundBufferStored) {
    renderer.restoreBwBuffer();
    backgroundAvailable = true;
  } else if (shouldUseReaderPageBackground && !path.empty()) {
    backgroundWasRebuilt = renderSavedReaderPage();
    backgroundAvailable = backgroundWasRebuilt;

    if (!backgroundWasRebuilt) {
      LOG_DBG("SLP", "Page re-render failed, using white background");
      renderer.clearScreen();
    }
  } else {
    LOG_DBG("SLP", "No current screen snapshot available for overlay sleep screen");
    renderer.clearScreen();
  }

  // Remove the live battery strip from the preserved/reconstructed reader page so the
  // overlay sleep screen still shows chapter/progress details without the battery glance target.
  if (shouldUseReaderPageBackground && backgroundAvailable) {
    hideOverlayBatteryStrip(renderer);
  }

  // Step 2: Load the overlay image using the same selection logic as renderCustomSleepScreen.
  // BMP: white pixels are skipped (transparent via drawBitmap), black pixels composited on top.
  // PNG: pixels with alpha < 128 are skipped; opaque pixels are drawn with their grayscale value.
  auto tryDrawOverlay = [&](const std::string& filename) -> OverlayDrawResult {
    FsFile file;
    if (!Storage.openFileForRead("SLP", filename, file)) {
      if (Storage.exists(filename.c_str())) {
        LOG_ERR("SLP", "BMP overlay exists but could not be opened: %s", filename.c_str());
        return OverlayDrawResult::Failed;
      }
      LOG_DBG("SLP", "BMP overlay not found: %s", filename.c_str());
      return OverlayDrawResult::NotFound;
    }
    Bitmap bitmap(file, true);
    const BmpReaderError parseResult = bitmap.parseHeaders();
    if (parseResult != BmpReaderError::Ok) {
      LOG_ERR("SLP", "BMP overlay header parse failed for %s: %s", filename.c_str(),
              Bitmap::errorToString(parseResult));
      file.close();
      return OverlayDrawResult::Failed;
    }

    int x, y;
    float cropX = 0, cropY = 0;
    if (bitmap.getWidth() > pageWidth || bitmap.getHeight() > pageHeight) {
      float ratio = static_cast<float>(bitmap.getWidth()) / static_cast<float>(bitmap.getHeight());
      const float screenRatio = static_cast<float>(pageWidth) / static_cast<float>(pageHeight);
      if (ratio > screenRatio) {
        x = 0;
        y = std::round((static_cast<float>(pageHeight) - static_cast<float>(pageWidth) / ratio) / 2);
      } else {
        x = std::round((static_cast<float>(pageWidth) - static_cast<float>(pageHeight) * ratio) / 2);
        y = 0;
      }
    } else {
      x = (pageWidth - bitmap.getWidth()) / 2;
      y = (pageHeight - bitmap.getHeight()) / 2;
    }

    // Draw without clearScreen so the reader page remains in the frame buffer beneath
    LOG_INF("SLP", "Drawing BMP overlay: %s", filename.c_str());
    renderer.drawBitmap(bitmap, x, y, pageWidth, pageHeight, cropX, cropY);
    file.close();
    return OverlayDrawResult::Drawn;
  };

  auto tryDrawPngOverlay = [&](const std::string& filename) -> OverlayDrawResult {
    if (!Storage.exists(filename.c_str())) {
      LOG_DBG("SLP", "PNG overlay not found: %s", filename.c_str());
      return OverlayDrawResult::NotFound;
    }

    constexpr size_t MIN_FREE_HEAP = 60 * 1024;  // PNG decoder ~42 KB + overhead
    if (ESP.getFreeHeap() < MIN_FREE_HEAP) {
      LOG_ERR("SLP", "Not enough heap for PNG overlay decoder: %u free, need %u for %s", ESP.getFreeHeap(),
              static_cast<unsigned>(MIN_FREE_HEAP), filename.c_str());
      return OverlayDrawResult::Failed;
    }
    PNG* png = new (std::nothrow) PNG();
    if (!png) {
      LOG_ERR("SLP", "Failed to allocate PNG overlay decoder for %s", filename.c_str());
      return OverlayDrawResult::Failed;
    }

    int rc = png->open(filename.c_str(), pngSleepOpen, pngSleepClose, pngSleepRead, pngSleepSeek, pngOverlayDraw);
    if (rc != PNG_SUCCESS) {
      delete png;
      LOG_ERR("SLP", "PNG overlay open failed for %s: %d", filename.c_str(), rc);
      return OverlayDrawResult::Failed;
    }

    const int srcW = png->getWidth(), srcH = png->getHeight();
    float yScale = 1.0f;
    int dstW = srcW, dstH = srcH;
    if (srcW > pageWidth || srcH > pageHeight) {
      const float scaleX = (float)pageWidth / srcW, scaleY = (float)pageHeight / srcH;
      const float scale = (scaleX < scaleY) ? scaleX : scaleY;
      dstW = (int)(srcW * scale);
      dstH = (int)(srcH * scale);
      yScale = (float)dstH / srcH;
    }

    PngOverlayCtx ctx;
    ctx.renderer = &renderer;
    ctx.screenW = pageWidth;
    ctx.screenH = pageHeight;
    ctx.srcWidth = srcW;
    ctx.dstWidth = dstW;
    ctx.dstX = (pageWidth - dstW) / 2;
    ctx.dstY = (pageHeight - dstH) / 2;
    ctx.yScale = yScale;
    ctx.lastDstY = -1;
    ctx.transparentColor = -2;  // will be resolved on first draw callback (after tRNS is parsed)
    ctx.pngObj = png;

    LOG_INF("SLP", "Drawing PNG overlay: %s", filename.c_str());
    rc = png->decode(&ctx, 0);
    png->close();
    delete png;
    if (rc != PNG_SUCCESS) {
      LOG_ERR("SLP", "PNG overlay decode failed for %s: %d", filename.c_str(), rc);
      return OverlayDrawResult::Failed;
    }
    return OverlayDrawResult::Drawn;
  };

  bool overlayDrawn = false;
  bool overlayCandidateFailed = false;
  SleepImageSelection selection;
  auto trySelectedOverlay = [&](const SleepImageSelection& image) {
    LOG_INF("SLP", "Selected overlay image: %s", image.path.c_str());
    const OverlayDrawResult result = image.isPng ? tryDrawPngOverlay(image.path) : tryDrawOverlay(image.path);
    overlayDrawn = result == OverlayDrawResult::Drawn;
    overlayCandidateFailed = overlayCandidateFailed || result == OverlayDrawResult::Failed;
  };

  if (selectPinnedSleepImage(SleepImageMode::Overlay, selection)) {
    trySelectedOverlay(selection);
  }
  if (!overlayDrawn && selectRandomSleepImage(SleepImageMode::Overlay, selection)) {
    trySelectedOverlay(selection);
  }

  if (!overlayDrawn) {
    const OverlayDrawResult result = tryDrawOverlay("/sleep.bmp");
    overlayDrawn = result == OverlayDrawResult::Drawn;
    overlayCandidateFailed = overlayCandidateFailed || result == OverlayDrawResult::Failed;
  }
  if (!overlayDrawn) {
    const OverlayDrawResult result = tryDrawPngOverlay("/sleep.png");
    overlayDrawn = result == OverlayDrawResult::Drawn;
    overlayCandidateFailed = overlayCandidateFailed || result == OverlayDrawResult::Failed;
  }

  if (!overlayDrawn) {
    if (overlayCandidateFailed) {
      LOG_ERR("SLP", "Overlay image was found but could not be drawn; falling back to default sleep screen");
      renderer.setOrientation(savedOrientation);
      return renderDefaultSleepScreen();
    }
    if (!backgroundAvailable) {
      LOG_DBG("SLP", "No overlay image or current screen snapshot available, falling back to default sleep screen");
      renderer.setOrientation(savedOrientation);
      return renderDefaultSleepScreen();
    }
    LOG_DBG("SLP", "No overlay image found, displaying background without overlay");
  }

  renderer.setOrientation(savedOrientation);
  // The grayscale re-render has no mask for the overlay image. If an overlay was
  // drawn, keep the composited BW frame intact instead of painting page glyphs
  // over the sleep image.
  const bool shouldRunGrayscalePass = shouldUseReaderPageBackground && backgroundSupportsGrayscale && !overlayDrawn &&
                                      (backgroundWasRebuilt || (overlayBackgroundBufferStored && !path.empty()));
  renderer.displayBuffer(HalDisplay::FULL_REFRESH, !shouldRunGrayscalePass && TURN_OFF_SCREEN_AFTER_SLEEP_REFRESH);

  if (!shouldRunGrayscalePass) {
    return;
  }

  if (!renderer.storeBwBuffer()) {
    LOG_ERR("SLP", "Overlay: failed to store BW buffer for grayscale pass");
    return;
  }

  renderer.setRenderMode(GfxRenderer::GRAYSCALE_LSB);
  if (!renderSavedReaderPage()) {
    LOG_ERR("SLP", "Overlay: failed to rebuild page for grayscale LSB pass");
    renderer.setRenderMode(GfxRenderer::BW);
    renderer.restoreBwBuffer();
    return;
  }
  renderer.copyGrayscaleLsbBuffers();

  renderer.setRenderMode(GfxRenderer::GRAYSCALE_MSB);
  if (!renderSavedReaderPage()) {
    LOG_ERR("SLP", "Overlay: failed to rebuild page for grayscale MSB pass");
    renderer.setRenderMode(GfxRenderer::BW);
    renderer.restoreBwBuffer();
    return;
  }
  renderer.copyGrayscaleMsbBuffers();

  renderer.displayGrayBuffer(TURN_OFF_SCREEN_AFTER_SLEEP_REFRESH);
  renderer.setRenderMode(GfxRenderer::BW);
  renderer.restoreBwBuffer();
}
