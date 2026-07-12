#include "PetManager.h"

#include <Arduino.h>
#include <I18n.h>
#include <Logging.h>

// User-facing pet action methods.
// Each returns true if the action succeeded, false if blocked.
// Sets lastFeedback for UI display after every attempt.

bool PetManager::feedMeal() {
  if (!state.exists() || !state.isAlive()) { lastFeedback = tr(STR_PET_NO_PET_ERR); return false; }
  if (state.isSleeping) { lastFeedback = tr(STR_PET_BLOCKED_SLEEPING); return false; }
  if (state.isSick)     { lastFeedback = tr(STR_PET_BLOCKED_SICK); return false; }

  if (state.waterStock == 0) {
    lastFeedback = tr(STR_PET_NO_WATER);
    return false;
  }
  state.waterStock--;

  state.hunger = clampAdd(state.hunger, PetConfig::HUNGER_PER_MEAL);
  state.weight = clampAdd(state.weight, PetConfig::WEIGHT_PER_MEAL);
  if (state.health < PetConfig::MAX_STAT && state.hunger > 0)
    state.health = clampAdd(state.health, 5);

  state.mealsSinceClean++;
  if (state.mealsSinceClean >= PetConfig::MEALS_UNTIL_WASTE) {
    state.mealsSinceClean = 0;
    if (state.wasteCount < PetConfig::MAX_WASTE) state.wasteCount++;
  }

  resetMissionsIfNewDay();
  if (state.missionWaterCount < 3) {
    state.missionWaterCount++;
    if (state.missionWaterCount >= 3 && !state.questWaterClaimed) {
      state.inkPoints += 20;
      state.questWaterClaimed = true;
    }
  }

  lastFeedback = tr(STR_PET_FED_MEAL);
  LOG_DBG("PET", "feedMeal: hunger=%d weight=%d", state.hunger, state.weight);
  save();
  return true;
}

bool PetManager::feedSnack() {
  if (!state.exists() || !state.isAlive()) { lastFeedback = tr(STR_PET_NO_PET_ERR); return false; }
  if (state.isSleeping) { lastFeedback = tr(STR_PET_BLOCKED_SLEEPING); return false; }
  if (state.isSick)     { lastFeedback = tr(STR_PET_BLOCKED_SICK); return false; }

  uint8_t boost = state.hasPremiumSprayer ? 10 : 0;
  state.happiness = clampAdd(state.happiness, PetConfig::HAPPINESS_PER_SNACK + boost);
  state.weight    = clampAdd(state.weight, PetConfig::WEIGHT_PER_SNACK);

  resetMissionsIfNewDay();
  if (state.missionWaterCount < 3) {
    state.missionWaterCount++;
    if (state.missionWaterCount >= 3 && !state.questWaterClaimed) {
      state.inkPoints += 20;
      state.questWaterClaimed = true;
    }
  }

  lastFeedback = tr(STR_PET_FED_SNACK);
  save();
  return true;
}

bool PetManager::giveMedicine() {
  if (!state.exists() || !state.isAlive()) { lastFeedback = tr(STR_PET_NO_PET_ERR); return false; }
  if (state.isSleeping) { lastFeedback = tr(STR_PET_BLOCKED_SLEEPING); return false; }
  if (!state.isSick)    { lastFeedback = tr(STR_PET_NOT_SICK); return false; }

  state.isSick = false;
  state.sicknessTimer = 0;
  lastFeedback = tr(STR_PET_GAVE_MEDICINE);
  LOG_DBG("PET", "giveMedicine: cured sickness");
  save();
  return true;
}

bool PetManager::exercise() {
  if (!state.exists() || !state.isAlive()) { lastFeedback = tr(STR_PET_NO_PET_ERR); return false; }
  if (state.isSleeping) { lastFeedback = tr(STR_PET_BLOCKED_SLEEPING); return false; }
  if (state.isSick)     { lastFeedback = tr(STR_PET_BLOCKED_EXERCISE); return false; }

  unsigned long now = millis();
  if (now - lastExerciseMs < PetConfig::EXERCISE_COOLDOWN_MS) {
    lastFeedback = tr(STR_PET_BLOCKED_COOLDOWN);
    return false;
  }

  lastExerciseMs = now;
  state.weight    = clampSub(state.weight, PetConfig::WEIGHT_PER_EXERCISE);
  state.happiness = clampAdd(state.happiness, 10);



  lastFeedback = tr(STR_PET_EXERCISED);
  LOG_DBG("PET", "exercise: weight=%d", state.weight);
  save();
  return true;
}

bool PetManager::cleanBathroom() {
  if (!state.exists() || !state.isAlive()) { lastFeedback = tr(STR_PET_NO_PET_ERR); return false; }
  if (state.wasteCount == 0) { lastFeedback = tr(STR_PET_ALREADY_CLEAN); return false; }



  lastFeedback = tr(STR_PET_CLEANED);
  LOG_DBG("PET", "cleanBathroom: removed %d waste piles", state.wasteCount);
  state.wasteCount = 0;
  save();
  return true;
}

bool PetManager::disciplinePet() {
  if (!state.exists() || !state.isAlive()) { lastFeedback = tr(STR_PET_NO_PET_ERR); return false; }
  if (state.isSleeping) { lastFeedback = tr(STR_PET_BLOCKED_SLEEPING); return false; }

  if (state.fertilizerStock == 0) {
    lastFeedback = tr(STR_PET_NO_FERTILIZER);
    return false;
  }
  state.fertilizerStock--;

  state.discipline = clampAdd(state.discipline, 25);

  if (state.attentionCall && state.currentNeed == PetNeed::NONE) {
    state.attentionCall = false;
  }

  lastFeedback = tr(STR_PET_SCOLDED_GOOD);
  save();
  return true;
}

bool PetManager::ignoreCry() {
  if (!state.exists() || !state.isAlive()) { lastFeedback = tr(STR_PET_NO_PET_ERR); return false; }
  if (!state.attentionCall) { lastFeedback = tr(STR_PET_NOTHING_IGNORE); return false; }

  if (state.isFakeCall) {
    // Ignoring fake cry = good training
    state.discipline = clampAdd(state.discipline, PetConfig::DISCIPLINE_PER_IGNORE_FAKE);
    lastFeedback = tr(STR_PET_IGNORED_GOOD);
  } else {
    // Ignoring a real need = neglect
    if (state.careMistakes < 255) state.careMistakes++;
    state.happiness = clampSub(state.happiness, 10);
    lastFeedback = tr(STR_PET_IGNORED_BAD);
  }

  state.attentionCall = false;
  state.isFakeCall = false;
  state.currentNeed = PetNeed::NONE;
  save();
  return true;
}

bool PetManager::toggleLights() {
  if (!state.exists() || !state.isAlive()) { lastFeedback = tr(STR_PET_NO_PET_ERR); return false; }

  state.lightsOff = state.lightsOff ? 0 : 1;
  lastFeedback = state.lightsOff ? tr(STR_PET_LIGHTS_OFF) : tr(STR_PET_LIGHTS_ON);
  save();
  return true;
}
