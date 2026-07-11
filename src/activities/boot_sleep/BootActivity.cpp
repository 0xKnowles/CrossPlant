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

  constexpr int PET_SCALE = 4;
  const int petSize = PetSpriteRenderer::displaySize(PET_SCALE);
  const int spriteX = (pageWidth - petSize) / 2;
  const int spriteY = (pageHeight - petSize) / 2 - 40;

  PetSpriteRenderer::drawPet(renderer, spriteX, spriteY, PetStage::COMPANION, PetMood::HAPPY, PET_SCALE,
                             0, 0, 0, true, false);

  renderer.drawCenteredText(UI_12_FONT_ID, spriteY + petSize + 24, "CrossMerge", true, EpdFontFamily::BOLD);
  renderer.drawCenteredText(UI_10_FONT_ID, spriteY + petSize + 24 + 30, tr(STR_BOOTING));
  renderer.drawCenteredText(UI_10_FONT_ID, pageHeight - 50, "v0.1.2");
  renderer.displayBuffer();
}
