#include "VirtualPetActivity.h"

#include <Arduino.h>
#include <I18n.h>

#include <cstring>

#include "activities/reader/GlobalReadingStats.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "pet/PetManager.h"
#include "pet/PetState.h"

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
      typeSelectIndex = (typeSelectIndex > 0) ? typeSelectIndex - 1 : 3;
      requestUpdate();
    });
    buttonNavigator.onNext([this] {
      typeSelectIndex = (typeSelectIndex + 1) % 4;
      requestUpdate();
    });
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      buyShopItem(typeSelectIndex);
      requestUpdate();
    }
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    PET_MANAGER.save();
    finish();
    return;
  }

  if (!PET_MANAGER.exists() || !PET_MANAGER.isAlive()) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      startHatchFlow();
    }
    return;
  }

  bool changed = false;
  buttonNavigator.onPrevious([&] {
    actionMenu.moveUp();
    changed = true;
  });
  buttonNavigator.onNext([&] {
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
    case PetAction::RENAME:        startRenameFlow();           return;
    case PetAction::CHANGE_TYPE:   startTypeSelectForChange();  return;
    case PetAction::SHOP:          typeSelectIndex = 0; screenMode = ScreenMode::SHOP; return;
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
  const auto& state = PET_MANAGER.getState();
  if (index == 0) {
    // Treat Box (20 IP)
    if (state.inkPoints >= 20) {
      if (PET_MANAGER.feedMeal()) {
        PET_MANAGER.deductPoints(20);
        PET_MANAGER.save();
      }
    }
  } else if (index == 1) {
    // Reading Toy (50 IP)
    if (!state.hasToy && state.inkPoints >= 50) {
      PET_MANAGER.deductPoints(50);
      PET_MANAGER.setHasToy(true);
    }
  } else if (index == 2) {
    // Round Glasses (100 IP)
    if (!state.hasGlasses) {
      if (state.inkPoints >= 100) {
        PET_MANAGER.deductPoints(100);
        PET_MANAGER.setHasGlasses(true);
        PET_MANAGER.setEquipGlasses(true);
      }
    } else {
      PET_MANAGER.setEquipGlasses(!state.equipGlasses);
    }
  } else if (index == 3) {
    // Wizard Hat (150 IP)
    if (!state.hasHat) {
      if (state.inkPoints >= 150) {
        PET_MANAGER.deductPoints(150);
        PET_MANAGER.setHasHat(true);
        PET_MANAGER.setEquipHat(true);
      }
    } else {
      PET_MANAGER.setEquipHat(!state.equipHat);
    }
  }
}
