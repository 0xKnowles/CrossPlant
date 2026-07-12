#include "VirtualPetActivity.h"

#include <Arduino.h>
#include <I18n.h>

#include <cstring>

#include "activities/reader/GlobalReadingStats.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "activities/util/ConfirmationActivity.h"
#include "pet/PetManager.h"
#include "pet/PetState.h"

#ifndef SIMULATOR
#include <WiFi.h>
#include "activities/network/WifiSelectionActivity.h"
#endif

// ---- Lifecycle ----------------------------------------------------------

void VirtualPetActivity::onEnter() {
  Activity::onEnter();
  PET_MANAGER.load();
  PET_MANAGER.syncFromReadingStats(GlobalReadingStats::load());
  PET_MANAGER.tick();
  PET_MANAGER.save();
  lastAnimMs = millis();
  requestUpdate();
}

void VirtualPetActivity::loop() {
  // Type selection mode: handle Up/Down/Confirm/Back inline
  if (screenMode == ScreenMode::TYPE_SELECT) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      screenMode = ScreenMode::NORMAL;
      requestUpdate();
      return;
    }
    buttonNavigator.onPrevious([this] {
      typeSelectIndex = (typeSelectIndex > 0) ? typeSelectIndex - 1 : PetTypeNames::COUNT - 1;
      requestUpdate();
    });
    buttonNavigator.onNext([this] {
      typeSelectIndex = (typeSelectIndex + 1) % PetTypeNames::COUNT;
      requestUpdate();
    });
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      confirmTypeSelect();
    }
    return;
  }

  // Shop mode: handle Up/Down/Confirm/Back inline
  if (screenMode == ScreenMode::SHOP) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      screenMode = ScreenMode::NORMAL;
      requestUpdate();
      return;
    }
    buttonNavigator.onPrevious([this] {
      typeSelectIndex = (typeSelectIndex > 0) ? typeSelectIndex - 1 : 6;
      requestUpdate();
    });
    buttonNavigator.onNext([this] {
      typeSelectIndex = (typeSelectIndex + 1) % 7;
      requestUpdate();
    });
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      buyShopItem(typeSelectIndex);
      requestUpdate();
    }
    return;
  }

  // Quests mode: any button returns to normal
  if (screenMode == ScreenMode::QUESTS) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back) ||
        mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      screenMode = ScreenMode::NORMAL;
      requestUpdate();
    }
    return;
  }

  // Herbarium mode: Up/Down cycles species, Back/Confirm exits
  if (screenMode == ScreenMode::ALBUM) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back) ||
        mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      screenMode = ScreenMode::NORMAL;
      requestUpdate();
    }
    buttonNavigator.onPrevious([this] {
      typeSelectIndex = (typeSelectIndex > 0) ? typeSelectIndex - 1 : 2;
      requestUpdate();
    });
    buttonNavigator.onNext([this] {
      typeSelectIndex = (typeSelectIndex + 1) % 3;
      requestUpdate();
    });
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    PET_MANAGER.save();
    finish();
    return;
  }

  // Front buttons (bezel, hinted Previous/Next) switch the active growing
  // plot; side buttons (volume rocker, Up/Down) drive the action menu below.
  // Plot switching works even before the active plot has been hatched, so
  // checked ahead of the "no pet" branch.
  if (PET_MANAGER.ownedPlotCount() > 1) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
      PET_MANAGER.switchPlot(-1);
      requestUpdate();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
      PET_MANAGER.switchPlot(1);
      requestUpdate();
      return;
    }
  }

  if (!PET_MANAGER.exists() || !PET_MANAGER.isAlive()) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      startHatchFlow();
    }
    return;
  }

  bool changed = false;
  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Up}, [&] {
    actionMenu.moveUp();
    changed = true;
  });
  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Down}, [&] {
    actionMenu.moveDown();
    changed = true;
  });
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    executeSelectedAction();
    changed = true;
  }

  // Animation timer: idle breathing + action icon expiry
  updateAnimation();

  if (changed) requestUpdate();
}

void VirtualPetActivity::executeSelectedAction() {
  const PetAction action = actionMenu.getSelected();
  switch (action) {
    case PetAction::FEED_MEAL:     PET_MANAGER.feedMeal();      triggerActionIcon(PetAnimIcon::FOOD);     break;
    case PetAction::FEED_SNACK:    PET_MANAGER.feedSnack();     triggerActionIcon(PetAnimIcon::SNACK);    break;
    case PetAction::MEDICINE:      PET_MANAGER.giveMedicine();  triggerActionIcon(PetAnimIcon::MEDICINE); break;
    case PetAction::EXERCISE:      PET_MANAGER.exercise();      triggerActionIcon(PetAnimIcon::EXERCISE); break;
    case PetAction::CLEAN:         PET_MANAGER.cleanBathroom(); triggerActionIcon(PetAnimIcon::CLEAN);    break;
    case PetAction::SCOLD:         PET_MANAGER.disciplinePet(); triggerActionIcon(PetAnimIcon::SCOLD);    break;
    case PetAction::IGNORE_CRY:    PET_MANAGER.ignoreCry();     break;
    case PetAction::TOGGLE_LIGHTS: PET_MANAGER.toggleLights();  triggerActionIcon(PetAnimIcon::SLEEP);    break;
    case PetAction::PET_PET:       PET_MANAGER.pet();           triggerActionIcon(PetAnimIcon::HEART);    break;
    case PetAction::DAILY_QUESTS:  screenMode = ScreenMode::QUESTS; return;
    case PetAction::RENAME:        startRenameFlow();           return;
    case PetAction::CHANGE_TYPE:   startTypeSelectForChange();  return;
    case PetAction::SHOP:          typeSelectIndex = 0; screenMode = ScreenMode::SHOP; return;
    case PetAction::ALBUM:         typeSelectIndex = 0; screenMode = ScreenMode::ALBUM; return;
    case PetAction::CONNECT_WEATHER: {
#ifdef SIMULATOR
      PET_MANAGER.updateWeather(1, 24);
      PET_MANAGER.setFeedback("Weather synced! (Simulated)");
#else
      if (WiFi.status() != WL_CONNECTED) {
        PET_MANAGER.setFeedback(tr(STR_PET_WEATHER_SYNCING));
        startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                               [this](const ActivityResult& result) {
                                 if (!result.isCancelled) {
                                   PET_MANAGER.forceWeatherSync();
                                   PET_MANAGER.tick();
                                 } else {
                                   PET_MANAGER.setFeedback(tr(STR_PET_WEATHER_FAILED));
                                 }
                               });
      } else {
        PET_MANAGER.setFeedback(tr(STR_PET_WEATHER_SYNCING));
        PET_MANAGER.forceWeatherSync();
        PET_MANAGER.tick();
      }
#endif
      return;
    }
    case PetAction::REFILL_WATER: {
      PET_MANAGER.refillWater();
      PET_MANAGER.setFeedback(tr(STR_PET_WATER_REFILLED));
      break;
    }
    case PetAction::BUY_FERTILIZER: {
      if (PET_MANAGER.getFarmState().inkPoints >= 30) {
        if (PET_MANAGER.getFarmState().fertilizerStock < 3) {
          PET_MANAGER.deductPoints(30);
          PET_MANAGER.refillFertilizer();
          PET_MANAGER.setFeedback(tr(STR_PET_FERTILIZER_BOUGHT));
        } else {
          PET_MANAGER.setFeedback(tr(STR_PET_FERTILIZER_FULL));
        }
      } else {
        PET_MANAGER.setFeedback(tr(STR_PET_NOT_ENOUGH_DD));
      }
      break;
    }
    case PetAction::RESET_DATA:    startResetFlow();            return;
    default: break;
  }
}

// ---- Animation helpers (render methods in VirtualPetActivityRender.cpp) ----

void VirtualPetActivity::updateAnimation() {
  const unsigned long now = millis();

  // Action icon expiry
  if (actionIcon != PetAnimIcon::NONE && now >= actionIconEndMs) {
    actionIcon = PetAnimIcon::NONE;
    requestUpdate();
  }

  // Idle breathing / wobble toggle
  if (now - lastAnimMs >= ANIM_INTERVAL_MS) {
    lastAnimMs = now;
    animFrame = (animFrame + 1) % 3;
    requestUpdate();
  }
}

void VirtualPetActivity::triggerActionIcon(PetAnimIcon icon) {
  actionIcon = icon;
  actionIconEndMs = millis() + ACTION_ICON_DURATION_MS;
}

bool VirtualPetActivity::animActive() const {
  return actionIcon != PetAnimIcon::NONE;
}

// ---- Hatch / rename / type-change flow ------------------------------------

void VirtualPetActivity::startHatchFlow() {
  // Step 1: ask for a name via keyboard
  const char* currentName = PET_MANAGER.exists() ? PET_MANAGER.getState().petName : "";
  startActivityForResult(
      std::make_unique<KeyboardEntryActivity>(renderer, mappedInput,
                                              tr(STR_PET_ENTER_NAME), currentName, 19, InputType::Text),
      [this](const ActivityResult& result) {
        if (!result.isCancelled) {
          auto text = std::get<KeyboardResult>(result.data).text;
          strncpy(pendingName, text.c_str(), sizeof(pendingName) - 1);
          pendingName[sizeof(pendingName) - 1] = '\0';
        } else {
          pendingName[0] = '\0';
        }
        // Step 2: show type selection
        startTypeSelectForHatch();
      });
}

void VirtualPetActivity::startRenameFlow() {
  const char* currentName = PET_MANAGER.getState().petName;
  startActivityForResult(
      std::make_unique<KeyboardEntryActivity>(renderer, mappedInput,
                                              tr(STR_PET_ENTER_NAME), currentName, 19, InputType::Text),
      [this](const ActivityResult& result) {
        if (!result.isCancelled) {
          auto text = std::get<KeyboardResult>(result.data).text;
          PET_MANAGER.renamePet(text.c_str());
        }
        screenMode = ScreenMode::NORMAL;
        requestUpdate();
      });
}

void VirtualPetActivity::startResetFlow() {
  startActivityForResult(
      std::make_unique<ConfirmationActivity>(
          renderer, mappedInput, tr(STR_PET_RESET_CONFIRM_TITLE),
          tr(STR_PET_RESET_CONFIRM_BODY), /*ignoreInitialConfirmRelease=*/false),
      [this](const ActivityResult& result) {
        if (!result.isCancelled) {
          PET_MANAGER.resetData();
        }
        screenMode = ScreenMode::NORMAL;
        requestUpdate();
      });
}

void VirtualPetActivity::startTypeSelectForHatch() {
  hatchAfterTypeSelect = true;
  typeSelectIndex = 0;
  screenMode = ScreenMode::TYPE_SELECT;
  requestUpdate();
}

void VirtualPetActivity::startTypeSelectForChange() {
  hatchAfterTypeSelect = false;
  typeSelectIndex = static_cast<int>(PET_MANAGER.getState().petType);
  screenMode = ScreenMode::TYPE_SELECT;
  requestUpdate();
}

void VirtualPetActivity::confirmTypeSelect() {
  const uint8_t selectedType = static_cast<uint8_t>(typeSelectIndex);
  if (hatchAfterTypeSelect) {
    PET_MANAGER.hatchNew(pendingName, selectedType);
    pendingName[0] = '\0';
  } else {
    PET_MANAGER.changeType(selectedType);
  }
  screenMode = ScreenMode::NORMAL;
  requestUpdate();
}

void VirtualPetActivity::buyShopItem(int index) {
  const auto& state = PET_MANAGER.getFarmState();
  if (index == 0) {
    // Moss Pole (250 DD)
    if (!state.hasMossPole) {
      if (state.inkPoints >= 250) {
        PET_MANAGER.deductPoints(250);
        PET_MANAGER.setHasMossPole(true);
        PET_MANAGER.setEquipMossPole(true);
      }
    } else {
      PET_MANAGER.setEquipMossPole(!state.equipMossPole);
    }
  } else if (index == 1) {
    // Self-Watering Pot (400 DD)
    if (!state.hasSelfWateringPot) {
      if (state.inkPoints >= 400) {
        PET_MANAGER.deductPoints(400);
        PET_MANAGER.setHasSelfWateringPot(true);
        PET_MANAGER.setEquipSelfWateringPot(true);
      }
    } else {
      PET_MANAGER.setEquipSelfWateringPot(!state.equipSelfWateringPot);
    }
  } else if (index == 2) {
    // Slow-Release Fertilizer (500 DD)
    if (!state.hasSlowReleaseFertilizer) {
      if (state.inkPoints >= 500) {
        PET_MANAGER.deductPoints(500);
        PET_MANAGER.setHasSlowReleaseFertilizer(true);
        PET_MANAGER.setEquipSlowReleaseFertilizer(true);
      }
    } else {
      PET_MANAGER.setEquipSlowReleaseFertilizer(!state.equipSlowReleaseFertilizer);
    }
  } else if (index == 3) {
    // Greenhouse Cover (650 DD)
    if (!state.hasGreenhouseCover) {
      if (state.inkPoints >= 650) {
        PET_MANAGER.deductPoints(650);
        PET_MANAGER.setHasGreenhouseCover(true);
        PET_MANAGER.setEquipGreenhouseCover(true);
      }
    } else {
      PET_MANAGER.setEquipGreenhouseCover(!state.equipGreenhouseCover);
    }
  } else if (index == 4) {
    // Premium Sprayer (300 DD)
    if (!state.hasPremiumSprayer && state.inkPoints >= 300) {
      PET_MANAGER.deductPoints(300);
      PET_MANAGER.setHasPremiumSprayer(true);
    }
  } else if (index == 5) {
    // Unlock Growing Plot 2 (800 DD)
    if (PET_MANAGER.ownedPlotCount() == 1) {
      PET_MANAGER.unlockNextPlot(800);
    }
  } else if (index == 6) {
    // Unlock Growing Plot 3 (1200 DD) — only offered once Plot 2 is owned
    if (PET_MANAGER.ownedPlotCount() == 2) {
      PET_MANAGER.unlockNextPlot(1200);
    }
  }
}
