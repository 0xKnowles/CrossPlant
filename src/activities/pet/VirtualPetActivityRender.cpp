#include "VirtualPetActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "components/UITheme.h"
#include "fontIds.h"
#include "pet/PetEvolution.h"
#include "pet/PetManager.h"
#include "pet/PetSpriteRenderer.h"
#include "pet/PetState.h"

// ---- Render entry point -------------------------------------------------

void VirtualPetActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight},
                 tr(STR_VIRTUAL_PET));

  if (screenMode == ScreenMode::TYPE_SELECT) {
    renderTypeSelect();
  } else if (screenMode == ScreenMode::SHOP) {
    renderShop();
  } else if (!PET_MANAGER.exists()) {
    renderNoPet();
  } else if (!PET_MANAGER.isAlive()) {
    renderDead();
  } else {
    renderAlive();
  }

  renderer.displayBuffer();
}

// ---- Sub-renderers ------------------------------------------------------

void VirtualPetActivity::renderNoPet() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageHeight = renderer.getScreenHeight();
  const int lh = renderer.getLineHeight(UI_10_FONT_ID);
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int centerY = contentTop + (pageHeight - contentTop - metrics.buttonHintsHeight) / 2;

  renderer.drawCenteredText(UI_10_FONT_ID, centerY - lh, tr(STR_PET_NO_PET));
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_PET_HATCH_EGG), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void VirtualPetActivity::renderDead() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageHeight = renderer.getScreenHeight();
  const int lh = renderer.getLineHeight(UI_10_FONT_ID);
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int centerY = contentTop + (pageHeight - contentTop - metrics.buttonHintsHeight) / 2;

  constexpr int PET_SCALE = 3;
  const int petSize = PetSpriteRenderer::displaySize(PET_SCALE);
  const int spriteX = (renderer.getScreenWidth() - petSize) / 2;
  PetSpriteRenderer::drawPet(renderer, spriteX, centerY - petSize - lh,
                              PetStage::DEAD, PetMood::DEAD, PET_SCALE, 0,
                              PET_MANAGER.getState().petType);
  renderer.drawCenteredText(UI_10_FONT_ID, centerY, tr(STR_PET_DEAD_MESSAGE));
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_PET_HATCH_NEW), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void VirtualPetActivity::renderAlive() const {
  const auto& state = PET_MANAGER.getState();
  const auto mood = PET_MANAGER.getMood();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentBottom = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing;

  // --- Left Column (Pet, Status Icons, Name, Reading Stats) ---
  const int col1W = 240;
  const int col1X = 15;

  // Pet sprite (scale 3 = 144x144)
  constexpr int PET_SCALE = 3;
  const int petSize = PetSpriteRenderer::displaySize(PET_SCALE);
  int spriteX = col1X + (col1W - petSize) / 2;
  int spriteY = contentTop;

  // Idle bobbing / wobble animation
  static const int EGG_WOBBLE[3] = {-2, 0, 2};
  static const int BOB[3] = {0, 3, 0};
  if (state.stage == PetStage::EGG) {
    spriteX += EGG_WOBBLE[animFrame];
  } else {
    spriteY += BOB[animFrame];
  }

  PetSpriteRenderer::drawPet(renderer, spriteX, spriteY, state.stage, mood, PET_SCALE,
                              state.evolutionVariant, state.petType, animFrame);

  // Draw action feedback icon next to sprite (top-right)
  if (actionIcon != PetAnimIcon::NONE) {
    drawPetActionIcon(renderer, actionIcon, spriteX + petSize + 4, contentTop + 8);
  }

  // Status icons
  const int iconsY = contentTop + petSize + 4;
  statsPanel.renderStatusIcons(renderer, state, col1X, iconsY, col1W);

  // Name & Stage
  const int infoY = iconsY + renderer.getLineHeight(SMALL_FONT_ID) + 6;
  const char* stageName = PetEvolution::variantStageName(state.stage, state.evolutionVariant);
  char nameStage[64];
  const char* petName = state.petName[0] ? state.petName : PetTypeNames::get(state.petType);
  snprintf(nameStage, sizeof(nameStage), "%s (%s)", petName, stageName);
  renderer.drawText(UI_10_FONT_ID, col1X + 10, infoY, nameStage, true, EpdFontFamily::BOLD);

  // Reading Stats (moved under the pet in the left column)
  const int rStatsY = infoY + renderer.getLineHeight(UI_10_FONT_ID) + 12;
  renderer.drawText(UI_10_FONT_ID, col1X + 10, rStatsY, "READING STATS", true, EpdFontFamily::BOLD);
  renderer.fillRect(col1X + 10, rStatsY + renderer.getLineHeight(UI_10_FONT_ID) + 2, col1W - 20, 1);

  const int rowSpacing = renderer.getLineHeight(SMALL_FONT_ID) + 4;
  const int rStatsLinesY = rStatsY + renderer.getLineHeight(UI_10_FONT_ID) + 8;

  char streakText[64];
  snprintf(streakText, sizeof(streakText), "Streak: %u days (Best: %u)", state.currentStreak, state.longestReadingStreak);
  renderer.drawText(SMALL_FONT_ID, col1X + 10, rStatsLinesY, streakText);

  char booksText[64];
  snprintf(booksText, sizeof(booksText), "Books Completed: %u", state.booksFinished);
  renderer.drawText(SMALL_FONT_ID, col1X + 10, rStatsLinesY + rowSpacing, booksText);

  char pagesText[64];
  snprintf(pagesText, sizeof(pagesText), "Total Pages: %lu", (unsigned long)state.totalPagesRead);
  renderer.drawText(SMALL_FONT_ID, col1X + 10, rStatsLinesY + rowSpacing * 2, pagesText);

  char sessionsText[64];
  snprintf(sessionsText, sizeof(sessionsText), "Sessions: %lu", (unsigned long)state.lastKnownSessions);
  renderer.drawText(SMALL_FONT_ID, col1X + 10, rStatsLinesY + rowSpacing * 3, sessionsText);

  // --- Vertical Divider ---
  renderer.drawLine(265, contentTop, 265, contentBottom, true);

  // --- Right Column (Pet Status, Compact Bars, Pet Actions Menu) ---
  const int col2X = 280;
  const int col2W = pageWidth - col2X - 15;

  // 1. Pet Vitals Vitals Box
  renderer.drawText(UI_10_FONT_ID, col2X, contentTop, "PET STATUS", true, EpdFontFamily::BOLD);
  renderer.fillRect(col2X, contentTop + renderer.getLineHeight(UI_10_FONT_ID) + 2, col2W, 1);

  const int barYStart = contentTop + renderer.getLineHeight(UI_10_FONT_ID) + 8;

  auto drawCompactBar = [&](const char* label, uint8_t val, int yOffset) {
    renderer.drawText(SMALL_FONT_ID, col2X + 10, yOffset, label);
    int labelW = renderer.getTextWidth(SMALL_FONT_ID, label);
    int barX = col2X + 10 + labelW + 6;
    int barW = col2W - 20 - labelW - 6;
    renderer.drawRect(barX, yOffset + 2, barW, 8);
    if (val > 0) {
      int fillW = (barW - 2) * val / 100;
      renderer.fillRect(barX + 1, yOffset + 3, fillW, 6);
    }
  };

  drawCompactBar("Hunger: ", state.hunger, barYStart);
  drawCompactBar("Happy:  ", state.happiness, barYStart + rowSpacing);
  drawCompactBar("Health: ", state.health, barYStart + rowSpacing * 2);
  drawCompactBar("Discip: ", state.discipline, barYStart + rowSpacing * 3);

  // Weight, Age
  char weightAgeLine[80];
  snprintf(weightAgeLine, sizeof(weightAgeLine), "Wt: %ug  |  Age: %lu days", state.weight, (unsigned long)PET_MANAGER.getDaysAlive());
  renderer.drawText(SMALL_FONT_ID, col2X + 10, barYStart + rowSpacing * 4 + 2, weightAgeLine);

  // Ink Points
  char ipLine[64];
  snprintf(ipLine, sizeof(ipLine), "Points: %lu IP", (unsigned long)state.inkPoints);
  renderer.drawText(UI_10_FONT_ID, col2X + 10, barYStart + rowSpacing * 5 + 4, ipLine, true, EpdFontFamily::BOLD);

  // 2. Pet Actions Menu
  const int menuHeaderY = barYStart + rowSpacing * 5 + 24;
  renderer.drawText(UI_10_FONT_ID, col2X, menuHeaderY, "PET ACTIONS", true, EpdFontFamily::BOLD);
  renderer.fillRect(col2X, menuHeaderY + renderer.getLineHeight(UI_10_FONT_ID) + 2, col2W, 1);

  const int menuY = menuHeaderY + renderer.getLineHeight(UI_10_FONT_ID) + 6;
  const int menuH = contentBottom - menuY - (PET_MANAGER.getLastFeedback() ? rowSpacing : 0);
  actionMenu.render(renderer, state, col2X, menuY, col2W, menuH);

  // --- Feedback text ---
  if (PET_MANAGER.getLastFeedback() != nullptr) {
    const int fbY = contentBottom - renderer.getLineHeight(SMALL_FONT_ID) - 2;
    renderer.drawText(SMALL_FONT_ID, col2X + 10, fbY, PET_MANAGER.getLastFeedback(), true, EpdFontFamily::BOLD);
  }

  // --- Button hints ---
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

// ---- Type selection screen -----------------------------------------------

void VirtualPetActivity::renderTypeSelect() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;

  renderer.drawCenteredText(SMALL_FONT_ID, contentTop, tr(STR_PET_SELECT_TYPE));

  const int lh = renderer.getLineHeight(UI_10_FONT_ID);
  const int rowH = lh + 8;
  const int listTop = contentTop + renderer.getLineHeight(SMALL_FONT_ID) + metrics.verticalSpacing;

  // Layout: left = sprite preview, right = type list
  constexpr int PREVIEW_SCALE = 3;
  const int previewSize = PetSpriteRenderer::displaySize(PREVIEW_SCALE);  // 144px
  const int previewX = metrics.contentSidePadding;
  const int previewY = listTop + 8;

  // Draw preview sprite of selected type at COMPANION stage (most recognizable)
  PetSpriteRenderer::drawPet(renderer, previewX, previewY, PetStage::COMPANION, PetMood::HAPPY,
                              PREVIEW_SCALE, 0, static_cast<uint8_t>(typeSelectIndex));

  // Draw selected type name below preview
  const char* selectedName = PetEvolution::typeName(typeSelectIndex);
  const int nameY = previewY + previewSize + 8;
  const int nameCenterX = previewX + previewSize / 2;
  int tw = renderer.getTextWidth(UI_10_FONT_ID, selectedName);
  renderer.drawText(UI_10_FONT_ID, nameCenterX - tw / 2, nameY, selectedName, false, EpdFontFamily::BOLD);

  // Type list on the right side
  const int listX = previewX + previewSize + 20;
  const int listW = pageWidth - listX - metrics.contentSidePadding;

  for (int i = 0; i < static_cast<int>(PetTypeNames::COUNT); i++) {
    const int rowY = listTop + i * rowH;
    if (rowY + rowH > pageHeight - metrics.buttonHintsHeight) break;

    const bool selected = (i == typeSelectIndex);
    if (selected) {
      renderer.fillRect(listX - 4, rowY - 2, listW + 8, rowH, true);
      renderer.drawText(UI_10_FONT_ID, listX, rowY, PetEvolution::typeName(i), /*black=*/false);
    } else {
      renderer.drawText(UI_10_FONT_ID, listX, rowY, PetEvolution::typeName(i));
    }
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void VirtualPetActivity::renderShop() const {
  const auto& state = PET_MANAGER.getState();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentBottom = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing;

  // Draw Shop Title and Balance
  char balanceLine[64];
  snprintf(balanceLine, sizeof(balanceLine), "Balance: %lu IP", (unsigned long)state.inkPoints);
  renderer.drawCenteredText(UI_10_FONT_ID, contentTop, "INK POINTS SHOP", true, EpdFontFamily::BOLD);
  renderer.drawCenteredText(UI_10_FONT_ID, contentTop + 24, balanceLine, true, EpdFontFamily::BOLD);

  const int lh = renderer.getLineHeight(UI_10_FONT_ID);
  const int rowH = lh + 10;
  const int listTop = contentTop + 55;

  struct ShopItem {
    const char* name;
    uint32_t cost;
    bool owned;
    bool equipped;
  };

  ShopItem items[4] = {
    {"Treat Box (Fills Hunger & Hp)", 20, false, false},
    {"Reading Toy (Passive: Halves Decay)", 50, state.hasToy, false},
    {"Round Glasses (Cosmetic)", 100, state.hasGlasses, state.equipGlasses},
    {"Wizard Hat (Cosmetic)", 150, state.hasHat, state.equipHat}
  };

  const int listX = metrics.contentSidePadding + 10;
  const int listW = pageWidth - listX * 2;

  for (int i = 0; i < 4; i++) {
    const int rowY = listTop + i * rowH;
    const bool selected = (i == typeSelectIndex);
    const auto& item = items[i];

    char itemText[128];
    if (i == 0) {
      snprintf(itemText, sizeof(itemText), "%s - %lu IP", item.name, (unsigned long)item.cost);
    } else if (item.owned) {
      if (i == 1) {
        snprintf(itemText, sizeof(itemText), "%s - Owned (Active)", item.name);
      } else {
        snprintf(itemText, sizeof(itemText), "%s - Owned (%s)", item.name, item.equipped ? "Equipped" : "Unequipped");
      }
    } else {
      snprintf(itemText, sizeof(itemText), "%s - %lu IP", item.name, (unsigned long)item.cost);
    }

    if (selected) {
      renderer.fillRect(listX - 4, rowY - 2, listW + 8, rowH, true);
      renderer.drawText(UI_10_FONT_ID, listX, rowY + 3, itemText, /*black=*/false);
    } else {
      renderer.drawText(UI_10_FONT_ID, listX, rowY + 3, itemText);
    }
  }

  // Draw description at the bottom
  const char* desc = "";
  if (typeSelectIndex == 0) desc = "A yummy treat box. Restores Hunger and Health.";
  if (typeSelectIndex == 1) desc = "A cute wooden toy. Halves your pet's happiness decay rate.";
  if (typeSelectIndex == 2) desc = "Cute round glasses. Buy or toggle equipping them.";
  if (typeSelectIndex == 3) desc = "A magical top hat. Buy or toggle equipping it.";

  const int descY = contentBottom - 30;
  renderer.drawCenteredText(SMALL_FONT_ID, descY, desc);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "Buy/Toggle", tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}
