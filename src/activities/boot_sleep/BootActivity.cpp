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

  constexpr int PET_SCALE = 3;
  const int petSize = PetSpriteRenderer::displaySize(PET_SCALE);
  const int spriteX = (pageWidth - petSize) / 2;
  const int spriteY = (pageHeight - petSize) / 2 - 20;

  PetSpriteRenderer::drawPet(renderer, spriteX, spriteY, PetStage::COMPANION, PetMood::HAPPY, PET_SCALE,
                             0, 0, 0);

  renderer.drawCenteredText(UI_10_FONT_ID, spriteY + petSize + 16, "CrossMerge", true, EpdFontFamily::BOLD);
  renderer.drawCenteredText(SMALL_FONT_ID, spriteY + petSize + 16 + 25, tr(STR_BOOTING));
  renderer.drawCenteredText(SMALL_FONT_ID, pageHeight - 30, "v0.1.0");
  renderer.displayBuffer();
}
