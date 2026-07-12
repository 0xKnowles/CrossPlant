#include "BootActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "AppVersion.h"
#include "fontIds.h"
#include "pet/PetSpriteRenderer.h"

void BootActivity::onEnter() {
  Activity::onEnter();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();

  // Three mature (baked stage 3) portraits side by side: Monstera, Begonia, Alocasia.
  constexpr int PORTRAIT_COUNT = 3;
  constexpr int BAKED_MATURE_STAGE = 3;
  constexpr int OUTER_MARGIN = 60;
  constexpr int GAP = 20;
  const int portraitSize = (pageWidth - OUTER_MARGIN - (PORTRAIT_COUNT - 1) * GAP) / PORTRAIT_COUNT;
  const int rowWidth = PORTRAIT_COUNT * portraitSize + (PORTRAIT_COUNT - 1) * GAP;
  const int rowX = (pageWidth - rowWidth) / 2;
  const int rowY = (pageHeight - portraitSize) / 2 - 40;

  const uint8_t speciesOrder[PORTRAIT_COUNT] = {0, 1, 2};  // Monstera, Begonia, Alocasia
  for (int i = 0; i < PORTRAIT_COUNT; i++) {
    const int x = rowX + i * (portraitSize + GAP);
    PetSpriteRenderer::drawBakedPortrait(renderer, speciesOrder[i], BAKED_MATURE_STAGE, x, rowY,
                                         portraitSize);
  }

  renderer.drawCenteredText(UI_12_FONT_ID, rowY + portraitSize + 24, "CrossPlant", true, EpdFontFamily::BOLD);
  renderer.drawCenteredText(UI_10_FONT_ID, rowY + portraitSize + 24 + 30, tr(STR_BOOTING));
  renderer.drawCenteredText(UI_10_FONT_ID, pageHeight - 50, "v1.12.0 by 0xKnowles");
  renderer.displayBuffer();
}
