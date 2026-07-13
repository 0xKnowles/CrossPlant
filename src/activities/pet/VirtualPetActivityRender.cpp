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
  } else if (screenMode == ScreenMode::QUESTS) {
    renderQuests();
  } else if (screenMode == ScreenMode::ALBUM) {
    renderAlbum();
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
  const auto& farm = PET_MANAGER.getFarmState();
  const auto mood = PET_MANAGER.getMood();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentBottom = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const bool isX3 = (pageWidth == 528);

  const int inset = metrics.contentSidePadding;

  // --- Column coordinates ---
  const int col1X = 15;
  const int col1W = isX3 ? 240 : 360;
  const int col2X = col1X + col1W + 15;
  const int col2W = pageWidth - col2X - 15;

  // Vertical layout for left column
  const int statusY = contentTop;
  const int statusH = isX3 ? 245 : 140;
  const int petY = statusY + statusH + 15;
  const int petH = contentBottom - petY;

  // 1. Status Card (Left Column, Top)
  renderer.drawRoundedRect(col1X, statusY, col1W, statusH, 1, 8, true);
  renderer.drawLine(col1X, statusY + 30, col1X + col1W - 1, statusY + 30, true);
  int statusHeaderW = renderer.getTextWidth(UI_10_FONT_ID, "PLANT STATUS", EpdFontFamily::BOLD);
  renderer.drawText(UI_10_FONT_ID, col1X + (col1W - statusHeaderW) / 2, statusY + 6, "PLANT STATUS", true, EpdFontFamily::BOLD);

  const int rowSpacing = renderer.getLineHeight(SMALL_FONT_ID) + 4;

  if (isX3) {
    // Progress bar drawing lambda
    auto drawCompactBar = [&](const char* label, uint8_t val, int yOffset) {
      renderer.drawText(SMALL_FONT_ID, col1X + 15, yOffset, label);
      int labelW = renderer.getTextWidth(SMALL_FONT_ID, label);
      int bx = col1X + 15 + labelW + 6;
      int bw = col1W - 30 - labelW - 6;
      renderer.drawRect(bx, yOffset + 2, bw, 8);
      if (val > 0) {
        int fillW = (bw - 2) * val / 100;
        renderer.fillRect(bx + 1, yOffset + 3, fillW, 6);
      }
    };

    drawCompactBar("Moisture: ", state.hunger, statusY + 42);
    drawCompactBar("Sunlight: ", state.happiness, statusY + 42 + rowSpacing);
    drawCompactBar("Health:   ", state.health, statusY + 42 + rowSpacing * 2);
    drawCompactBar("Nutrient: ", state.discipline, statusY + 42 + rowSpacing * 3);

    char weightAge[64];
    snprintf(weightAge, sizeof(weightAge), "Ht: %u cm  |  Age: %lu days", state.weight, (unsigned long)PET_MANAGER.getDaysAlive());
    int weightAgeW = renderer.getTextWidth(SMALL_FONT_ID, weightAge);
    renderer.drawText(SMALL_FONT_ID, col1X + (col1W - weightAgeW) / 2, statusY + 42 + rowSpacing * 4 + 4, weightAge);

    char ipLine[64];
    snprintf(ipLine, sizeof(ipLine), "Points: %lu $Dew", (unsigned long)farm.inkPoints);
    int ipLineW = renderer.getTextWidth(UI_10_FONT_ID, ipLine, EpdFontFamily::BOLD);
    renderer.drawText(UI_10_FONT_ID, col1X + (col1W - ipLineW) / 2, statusY + 42 + rowSpacing * 5 + 10, ipLine, true, EpdFontFamily::BOLD);

    // Weather bonus display
    char weatherLine[120];
    const char* wCond = "Weather: Offline";
    if (farm.weatherCondition == 1) wCond = "Weather: Sunny (Light Boost)";
    else if (farm.weatherCondition == 2) wCond = "Weather: Rainy (Humidity Boost)";
    else if (farm.weatherCondition == 3) wCond = "Weather: Cloudy (Nutrient Boost)";
    else if (farm.weatherCondition == 4) wCond = "Weather: Snowy (Greenhouse Boost)";
    snprintf(weatherLine, sizeof(weatherLine), "%s", wCond);
    int weatherLineW = renderer.getTextWidth(SMALL_FONT_ID, weatherLine);
    renderer.drawText(SMALL_FONT_ID, col1X + (col1W - weatherLineW) / 2, statusY + 42 + rowSpacing * 5 + 32, weatherLine);
  } else {
    // X4: 2 columns of progress bars
    int halfW = col1W / 2;
    // Hunger
    renderer.drawText(SMALL_FONT_ID, col1X + 10, statusY + 40, "Moisture:");
    renderer.drawRect(col1X + 80, statusY + 42, halfW - 90, 6);
    if (state.hunger > 0) {
      renderer.fillRect(col1X + 81, statusY + 43, (halfW - 92) * state.hunger / 100, 4);
    }

    // Happy
    renderer.drawText(SMALL_FONT_ID, col1X + 10, statusY + 65, "Sunlight:");
    renderer.drawRect(col1X + 80, statusY + 67, halfW - 90, 6);
    if (state.happiness > 0) {
      renderer.fillRect(col1X + 81, statusY + 68, (halfW - 92) * state.happiness / 100, 4);
    }

    // Health
    renderer.drawText(SMALL_FONT_ID, col1X + halfW + 10, statusY + 40, "Health:");
    renderer.drawRect(col1X + halfW + 80, statusY + 42, halfW - 90, 6);
    if (state.health > 0) {
      renderer.fillRect(col1X + halfW + 81, statusY + 43, (halfW - 92) * state.health / 100, 4);
    }

    // Discip
    renderer.drawText(SMALL_FONT_ID, col1X + halfW + 10, statusY + 65, "Nutrients:");
    renderer.drawRect(col1X + halfW + 80, statusY + 67, halfW - 90, 6);
    if (state.discipline > 0) {
      renderer.fillRect(col1X + halfW + 81, statusY + 68, (halfW - 92) * state.discipline / 100, 4);
    }

    char statsLine[120];
    snprintf(statsLine, sizeof(statsLine), "Ht: %u cm  |  Age: %lu days  |  %lu $Dew",
             state.weight, (unsigned long)PET_MANAGER.getDaysAlive(), (unsigned long)farm.inkPoints);
    int statsLineW = renderer.getTextWidth(SMALL_FONT_ID, statsLine);
    renderer.drawText(SMALL_FONT_ID, col1X + (col1W - statsLineW) / 2, statusY + 98, statsLine);

    // Weather bonus display
    char weatherLine[120];
    const char* wCond = "Weather: Offline";
    if (farm.weatherCondition == 1) wCond = "Weather: Sunny (Light Boost)";
    else if (farm.weatherCondition == 2) wCond = "Weather: Rainy (Humidity Boost)";
    else if (farm.weatherCondition == 3) wCond = "Weather: Cloudy (Nutrient Boost)";
    else if (farm.weatherCondition == 4) wCond = "Weather: Snowy (Greenhouse Boost)";
    snprintf(weatherLine, sizeof(weatherLine), "%s", wCond);
    int weatherLineW = renderer.getTextWidth(SMALL_FONT_ID, weatherLine);
    renderer.drawText(SMALL_FONT_ID, col1X + (col1W - weatherLineW) / 2, statusY + 116, weatherLine);
  }

  // 2. Pet Image Card (Left Column, Bottom)
  renderer.drawRoundedRect(col1X, petY, col1W, petH, 1, 8, true);
  renderer.drawLine(col1X, petY + 30, col1X + col1W - 1, petY + 30, true);

  const char* petName = state.petName[0] ? state.petName : PetTypeNames::get(state.petType);
  const char* stageName = PetEvolution::variantStageName(state.stage, state.evolutionVariant, state.petType);
  char nameStage[64];
  snprintf(nameStage, sizeof(nameStage), "%s (%s)", petName, stageName);
  int nameStageW = renderer.getTextWidth(UI_10_FONT_ID, nameStage, EpdFontFamily::BOLD);
  renderer.drawText(UI_10_FONT_ID, col1X + (col1W - nameStageW) / 2, petY + 6, nameStage, true, EpdFontFamily::BOLD);

  // Wobble / Bobbing animation
  constexpr int PET_SCALE = 3;
  const int petSize = PetSpriteRenderer::displaySize(PET_SCALE);
  int spriteX = col1X + (col1W - petSize) / 2;
  int spriteY = petY + (isX3 ? 55 : 35);

  static const int EGG_WOBBLE[3] = {-2, 0, 2};
  static const int BOB[3] = {0, 3, 0};
  if (state.stage == PetStage::EGG) {
    spriteX += EGG_WOBBLE[animFrame];
  } else {
    spriteY += BOB[animFrame];
  }

  PetSpriteRenderer::drawPet(renderer, spriteX, spriteY, state.stage, mood, PET_SCALE,
                             state.evolutionVariant, state.petType, animFrame);

  if (actionIcon != PetAnimIcon::NONE) {
    drawPetActionIcon(renderer, actionIcon, spriteX + petSize + 4, spriteY + 8);
  }

  // Plot indicator — "Plot X/Y" left-aligned directly under the sprite, shown
  // only once more than one growing plot is owned so the common single-plot
  // layout is unchanged.
  int iconsY = spriteY + petSize + (isX3 ? 12 : 4);
  if (PET_MANAGER.ownedPlotCount() > 1) {
    char plotBuf[24];
    snprintf(plotBuf, sizeof(plotBuf), "Plot %d/%d", PET_MANAGER.activePlotIndex() + 1, PET_MANAGER.ownedPlotCount());
    renderer.drawText(SMALL_FONT_ID, col1X + 10, iconsY, plotBuf);
    iconsY += renderer.getLineHeight(SMALL_FONT_ID) + 4;
  }

  // Status icons
  statsPanel.renderStatusIcons(renderer, state, col1X, iconsY, col1W);

  // Reading Stats at bottom on X3
  if (isX3) {
    const int rStatsY = iconsY + 28;
    int rStatsHeaderW = renderer.getTextWidth(SMALL_FONT_ID, "READING PROGRESS", EpdFontFamily::BOLD);
    renderer.drawText(SMALL_FONT_ID, col1X + (col1W - rStatsHeaderW) / 2, rStatsY, "READING PROGRESS", true, EpdFontFamily::BOLD);
    renderer.drawLine(col1X + 15, rStatsY + renderer.getLineHeight(SMALL_FONT_ID) + 2, col1X + col1W - 15, rStatsY + renderer.getLineHeight(SMALL_FONT_ID) + 2, true);

    const int rStatsLinesY = rStatsY + renderer.getLineHeight(SMALL_FONT_ID) + 6;
    char progressLine1[80];
    snprintf(progressLine1, sizeof(progressLine1), "Streak: %u days  |  Books: %u", farm.currentStreak, farm.booksFinished);
    int progressLine1W = renderer.getTextWidth(SMALL_FONT_ID, progressLine1);
    renderer.drawText(SMALL_FONT_ID, col1X + (col1W - progressLine1W) / 2, rStatsLinesY, progressLine1);

    char progressLine2[80];
    snprintf(progressLine2, sizeof(progressLine2), "Total Pages: %lu", (unsigned long)state.totalPagesRead);
    int progressLine2W = renderer.getTextWidth(SMALL_FONT_ID, progressLine2);
    renderer.drawText(SMALL_FONT_ID, col1X + (col1W - progressLine2W) / 2, rStatsLinesY + rowSpacing, progressLine2);
  }

  // 3. Pet Actions Card (Right Column)
  renderer.drawRoundedRect(col2X, contentTop, col2W, contentBottom - contentTop, 1, 8, true);
  renderer.drawLine(col2X, contentTop + 30, col2X + col2W - 1, contentTop + 30, true);
  int actionsHeaderW = renderer.getTextWidth(UI_10_FONT_ID, "PLANT ACTIONS", EpdFontFamily::BOLD);
  renderer.drawText(UI_10_FONT_ID, col2X + (col2W - actionsHeaderW) / 2, contentTop + 6, "PLANT ACTIONS", true, EpdFontFamily::BOLD);

  const int menuY = contentTop + 38;
  const int menuH = contentBottom - menuY - (PET_MANAGER.getLastFeedback() ? 32 : 12);
  actionMenu.render(renderer, state, farm, col2X, menuY, col2W, menuH);

  // Action Feedback Text
  if (PET_MANAGER.getLastFeedback() != nullptr) {
    const int fbY = contentBottom - renderer.getLineHeight(SMALL_FONT_ID) - 6;
    int feedbackW = renderer.getTextWidth(SMALL_FONT_ID, PET_MANAGER.getLastFeedback(), EpdFontFamily::BOLD);
    renderer.drawText(SMALL_FONT_ID, col2X + (col2W - feedbackW) / 2, fbY, PET_MANAGER.getLastFeedback(), true, EpdFontFamily::BOLD);
  }

  // Button hints
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_PREVIOUS), tr(STR_NEXT));
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
  const auto& state = PET_MANAGER.getFarmState();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentBottom = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing;

  // Draw Shop Title and Balance
  char balanceLine[64];
  snprintf(balanceLine, sizeof(balanceLine), "Balance: %lu $Dew", (unsigned long)state.inkPoints);
  renderer.drawCenteredText(UI_10_FONT_ID, contentTop, "TROPICAL DEW SHOP", true, EpdFontFamily::BOLD);
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

  const int ownedPlots = PET_MANAGER.ownedPlotCount();
  ShopItem items[7] = {
    {"Moss Pole (Passive Boost)", 250, state.hasMossPole, state.equipMossPole},
    {"Self-Watering Pot (Passive Boost)", 400, state.hasSelfWateringPot, state.equipSelfWateringPot},
    {"Slow-Release Fertilizer (Passive Boost)", 500, state.hasSlowReleaseFertilizer, state.equipSlowReleaseFertilizer},
    {"Greenhouse Cover (Passive Boost)", 650, state.hasGreenhouseCover, state.equipGreenhouseCover},
    {"Premium Sprayer (Passive Boost)", 300, state.hasPremiumSprayer, false},
    {"Growing Plot 2 (Grow a 2nd Crop)", 800, ownedPlots >= 2, false},
    {"Growing Plot 3 (Grow a 3rd Crop)", 1200, ownedPlots >= 3, false},
  };

  const int listX = metrics.contentSidePadding + 10;
  const int listW = pageWidth - listX * 2;

  for (int i = 0; i < 7; i++) {
    const int rowY = listTop + i * rowH;
    const bool selected = (i == typeSelectIndex);
    const auto& item = items[i];
    const bool isPlotItem = (i == 5 || i == 6);
    const bool plotLocked = (i == 6 && ownedPlots < 2);  // Plot 3 requires Plot 2 first

    char itemText[128];
    if (item.owned) {
      if (isPlotItem || i == 4) {
        snprintf(itemText, sizeof(itemText), "%s - Owned", item.name);
      } else {
        snprintf(itemText, sizeof(itemText), "%s - Owned (%s)", item.name, item.equipped ? "Equipped" : "Unequipped");
      }
    } else if (plotLocked) {
      snprintf(itemText, sizeof(itemText), "%s - Buy Plot 2 first", item.name);
    } else {
      snprintf(itemText, sizeof(itemText), "%s - %lu $Dew", item.name, (unsigned long)item.cost);
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
  if (typeSelectIndex == 0) desc = "Moss Pole: Halves sunlight (happiness) decay rate when equipped.";
  if (typeSelectIndex == 1) desc = "Self-Watering Pot: Halves soil moisture decay rate when equipped.";
  if (typeSelectIndex == 2) desc = "Slow-Release Fertilizer: Regens +1 nutrient every 2h when equipped.";
  if (typeSelectIndex == 3) desc = "Greenhouse Cover: Halves health decay rate when equipped.";
  if (typeSelectIndex == 4) desc = "Premium Sprayer: Provides +10 Sunlight/Happiness bonus on misting.";
  if (typeSelectIndex == 5) desc = "Unlocks a 2nd growing plot so you can grow another species alongside your first.";
  if (typeSelectIndex == 6) desc = "Unlocks a 3rd growing plot — grow one of each species at once.";

  renderer.drawCenteredText(SMALL_FONT_ID, contentBottom - 18, desc);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "Buy/Toggle", tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void VirtualPetActivity::renderQuests() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;

  renderer.drawCenteredText(UI_10_FONT_ID, contentTop, "DAILY QUESTS", true, EpdFontFamily::BOLD);

  const int listTop = contentTop + 30;

  PetMission missions[6];
  PET_MANAGER.getMissions(missions);

  const int listX = metrics.contentSidePadding + 15;
  const int listW = pageWidth - listX * 2;

  for (int i = 0; i < 6; i++) {
    const int rowY = listTop + i * 36;
    const auto& mission = missions[i];

    // Quest Title (using SMALL_FONT_ID for compactness to prevent wrapping)
    renderer.drawText(SMALL_FONT_ID, listX, rowY, mission.label, true, EpdFontFamily::BOLD);

    // Progress display directly under the title
    const int barX = listX;
    const int barY = rowY + 14;
    const int barW = 150;

    if (mission.done) {
      const char* compText = "COMPLETED (+$Dew Claimed)";
      renderer.drawText(SMALL_FONT_ID, listX, barY - 1, compText, true, EpdFontFamily::BOLD);
    } else {
      renderer.drawRect(barX, barY, barW, 6);
      if (mission.progress > 0) {
        int val = std::min<int>(mission.progress * 100 / mission.goal, 100);
        int fillW = (barW - 2) * val / 100;
        renderer.fillRect(barX + 1, barY + 1, fillW, 4);
      }
      char progressText[32];
      snprintf(progressText, sizeof(progressText), "%u/%u", mission.progress, mission.goal);
      renderer.drawText(SMALL_FONT_ID, barX + barW + 8, barY - 2, progressText);
    }
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void VirtualPetActivity::renderAlbum() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;

  const auto& farm = PET_MANAGER.getFarmState();

  // Count total unlocked
  int totalUnlocked = 0;
  for (int i = 0; i < 12; i++) {
    if ((farm.unlockedStages & (1 << i)) != 0) {
      totalUnlocked++;
    }
  }

  char titleText[64];
  snprintf(titleText, sizeof(titleText), "HERBARIUM (%d/12 DISCOVERED)", totalUnlocked);
  renderer.drawCenteredText(UI_10_FONT_ID, contentTop, titleText, true, EpdFontFamily::BOLD);

  const int listTop = contentTop + 35;
  const int col1W = 160;
  const int col2W = pageWidth - col1W - metrics.contentSidePadding * 2 - 20;
  const int col1X = metrics.contentSidePadding + 10;
  const int col2X = col1X + col1W + 10;

  // Render left side: Species list
  renderer.drawRoundedRect(col1X, listTop, col1W, 110, 1, 8, true);
  renderer.drawLine(col1X, listTop + 24, col1X + col1W - 1, listTop + 24, true);
  renderer.drawText(SMALL_FONT_ID, col1X + 8, listTop + 5, "SPECIES", true, EpdFontFamily::BOLD);

  const char* speciesNames[3] = {"Alocasia", "Begonia", "Monstera"};
  // This list is alphabetical (unlike the global petType order Monstera=0/Begonia=1/Alocasia=2),
  // so map each alphabetical row back to its petType for species-specific stage vocabulary below.
  const uint8_t speciesPetTypes[3] = {2, 1, 0};
  for (int i = 0; i < 3; i++) {
    const int rowY = listTop + 32 + i * 24;
    const bool selected = (i == typeSelectIndex);
    if (selected) {
      renderer.fillRect(col1X + 4, rowY - 2, col1W - 8, 20, true);
      renderer.drawText(SMALL_FONT_ID, col1X + 12, rowY + 1, speciesNames[i], /*black=*/false, EpdFontFamily::BOLD);
    } else {
      renderer.drawText(SMALL_FONT_ID, col1X + 12, rowY + 1, speciesNames[i], /*black=*/true, EpdFontFamily::REGULAR);
    }
  }

  // Render right side: Selected species stage progress
  renderer.drawRoundedRect(col2X, listTop, col2W, 110, 1, 8, true);
  renderer.drawLine(col2X, listTop + 24, col2X + col2W - 1, listTop + 24, true);
  
  char progressHeader[64];
  snprintf(progressHeader, sizeof(progressHeader), "%s DISCOVERIES", speciesNames[typeSelectIndex]);
  renderer.drawText(SMALL_FONT_ID, col2X + 8, listTop + 5, progressHeader, true, EpdFontFamily::BOLD);

  const uint8_t selectedPetType = speciesPetTypes[typeSelectIndex];
  const PetStage discoveryStages[4] = {PetStage::HATCHLING, PetStage::YOUNGSTER, PetStage::COMPANION,
                                       PetStage::ELDER};
  char stageNameBufs[4][24];
  const char* stageNames[4];
  for (int i = 0; i < 4; i++) {
    snprintf(stageNameBufs[i], sizeof(stageNameBufs[i]), "%d. %s", i + 1,
             PetEvolution::stageName(discoveryStages[i], selectedPetType));
    stageNames[i] = stageNameBufs[i];
  }
  for (int st = 1; st <= 4; st++) {
    const int rowY = listTop + 32 + (st - 1) * 18;
    uint8_t bit = (typeSelectIndex * 4) + (st - 1);
    bool unlocked = (farm.unlockedStages & (1 << bit)) != 0;

    renderer.drawText(SMALL_FONT_ID, col2X + 12, rowY, stageNames[st - 1]);
    if (unlocked) {
      renderer.drawText(SMALL_FONT_ID, col2X + col2W - 80, rowY, "DISCOVERED", true, EpdFontFamily::BOLD);
    } else {
      renderer.drawText(SMALL_FONT_ID, col2X + col2W - 80, rowY, "??? (Locked)", false);
    }
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "Select", tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}
