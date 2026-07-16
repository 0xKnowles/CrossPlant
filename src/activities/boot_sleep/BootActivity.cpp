#include "BootActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>

#include "AppVersion.h"
#include "fontIds.h"
#include "images/Seed144.h"
#include "pet/PetSpriteRenderer.h"

void BootActivity::onEnter() {
  Activity::onEnter();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();

  // 3x3 showcase grid: one column per species (Monstera, Begonia, Alocasia), rows pick specific
  // baked growth stages (1=Hatchling/Sprout ... 4=Elder/Prized) rather than a strict progression,
  // so the boot screen reads as a curated poster rather than a literal growth timeline.
  constexpr int ROWS = 3;
  constexpr int COLS = 3;
  constexpr uint8_t BAKED_STAGE_GRID[ROWS][COLS] = {
    {1, 1, 1},
    {3, 2, 2},
    {4, 4, 4},
  };
  constexpr uint8_t SPECIES_ORDER[COLS] = {0, 1, 2};  // Monstera, Begonia, Alocasia

  constexpr int GAP = 14;
  constexpr int OUTER_MARGIN = 50;
  constexpr int TOP_MARGIN = 30;
  constexpr int TEXT_BLOCK_H = 66;   // title + subtitle drawn below the grid
  constexpr int BOTTOM_MARGIN = 60;  // reserved for the version line + padding

  const int maxGridWidth = pageWidth - OUTER_MARGIN;
  const int maxGridHeight = pageHeight - TOP_MARGIN - TEXT_BLOCK_H - BOTTOM_MARGIN;
  const int sizeFromWidth = (maxGridWidth - (COLS - 1) * GAP) / COLS;
  const int sizeFromHeight = (maxGridHeight - (ROWS - 1) * GAP) / ROWS;
  const int portraitSize = std::min(sizeFromWidth, sizeFromHeight);

  const int gridW = COLS * portraitSize + (COLS - 1) * GAP;
  const int gridH = ROWS * portraitSize + (ROWS - 1) * GAP;
  const int gridX = (pageWidth - gridW) / 2;
  const int gridY = TOP_MARGIN + (maxGridHeight - gridH) / 2;

  for (int r = 0; r < ROWS; r++) {
    for (int c = 0; c < COLS; c++) {
      const int x = gridX + c * (portraitSize + GAP);
      const int y = gridY + r * (portraitSize + GAP);
      if (r == 0 && c == 0) {
        // Monstera has no unique baked stage-1 art (BakedPlantSprites.h aliases mon_1 to
        // begonia_1 as a placeholder), which would otherwise render identically to the
        // Begonia-1 cell right next to it. Show the actual seed art here instead.
        PetSpriteRenderer::drawBaked144Image(renderer, Seed, x, y, portraitSize);
      } else {
        PetSpriteRenderer::drawBakedPortrait(renderer, SPECIES_ORDER[c], BAKED_STAGE_GRID[r][c], x, y,
                                             portraitSize);
      }
    }
  }

  const int textTop = gridY + gridH + 24;
  renderer.drawCenteredText(UI_12_FONT_ID, textTop, "CrossPlant", true, EpdFontFamily::BOLD);
  renderer.drawCenteredText(UI_10_FONT_ID, textTop + 30, tr(STR_BOOTING));
  renderer.drawCenteredText(UI_10_FONT_ID, pageHeight - 50, "Build v0.1.4");
  renderer.displayBuffer();
}
