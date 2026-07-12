#include "PetManager.h"

#include <Arduino.h>
#include <I18n.h>
#include <Logging.h>

// User-facing pet action methods.
// Each returns true if the action succeeded, false if blocked.
// Sets lastFeedback for UI display after every attempt.
// Vitals/waste/discipline are per-plot; water/fertilizer stock, currency, and
// quest tracking are farm-level (shared across all owned plots).

bool PetManager::feedMeal() {
  PetState& plot = activePlot();
  if (!plot.exists() || !plot.isAlive()) { lastFeedback = tr(STR_PET_NO_PET_ERR); return false; }
  if (plot.isSleeping) { lastFeedback = tr(STR_PET_BLOCKED_SLEEPING); return false; }
  if (plot.isSick)     { lastFeedback = tr(STR_PET_BLOCKED_SICK); return false; }

  if (farm.waterStock == 0) {
    lastFeedback = tr(STR_PET_NO_WATER);
    return false;
  }
  farm.waterStock--;

  plot.hunger = clampAdd(plot.hunger, PetConfig::HUNGER_PER_MEAL);
  plot.weight = clampAdd(plot.weight, PetConfig::WEIGHT_PER_MEAL);
  if (plot.health < PetConfig::MAX_STAT && plot.hunger > 0)
    plot.health = clampAdd(plot.health, 5);

  plot.mealsSinceClean++;
  if (plot.mealsSinceClean >= PetConfig::MEALS_UNTIL_WASTE) {
    plot.mealsSinceClean = 0;
    if (plot.wasteCount < PetConfig::MAX_WASTE) plot.wasteCount++;
  }

  resetMissionsIfNewDay();
  if (farm.missionWaterCount < 3) {
    farm.missionWaterCount++;
    if (farm.missionWaterCount >= 3 && !farm.questWaterClaimed) {
      farm.inkPoints += 20;
      farm.questWaterClaimed = true;
    }
  }

  lastFeedback = tr(STR_PET_FED_MEAL);
  LOG_DBG("PET", "feedMeal: hunger=%d weight=%d", plot.hunger, plot.weight);
  save();
  return true;
}

bool PetManager::feedSnack() {
  PetState& plot = activePlot();
  if (!plot.exists() || !plot.isAlive()) { lastFeedback = tr(STR_PET_NO_PET_ERR); return false; }
  if (plot.isSleeping) { lastFeedback = tr(STR_PET_BLOCKED_SLEEPING); return false; }
  if (plot.isSick)     { lastFeedback = tr(STR_PET_BLOCKED_SICK); return false; }

  uint8_t boost = farm.hasPremiumSprayer ? 10 : 0;
  plot.happiness = clampAdd(plot.happiness, PetConfig::HAPPINESS_PER_SNACK + boost);
  plot.weight    = clampAdd(plot.weight, PetConfig::WEIGHT_PER_SNACK);

  resetMissionsIfNewDay();
  if (farm.missionWaterCount < 3) {
    farm.missionWaterCount++;
    if (farm.missionWaterCount >= 3 && !farm.questWaterClaimed) {
      farm.inkPoints += 20;
      farm.questWaterClaimed = true;
    }
  }

  lastFeedback = tr(STR_PET_FED_SNACK);
  save();
  return true;
}

bool PetManager::giveMedicine() {
  PetState& plot = activePlot();
  if (!plot.exists() || !plot.isAlive()) { lastFeedback = tr(STR_PET_NO_PET_ERR); return false; }
  if (plot.isSleeping) { lastFeedback = tr(STR_PET_BLOCKED_SLEEPING); return false; }
  if (!plot.isSick)    { lastFeedback = tr(STR_PET_NOT_SICK); return false; }

  plot.isSick = false;
  plot.sicknessTimer = 0;
  lastFeedback = tr(STR_PET_GAVE_MEDICINE);
  LOG_DBG("PET", "giveMedicine: cured sickness");
  save();
  return true;
}

bool PetManager::exercise() {
  PetState& plot = activePlot();
  if (!plot.exists() || !plot.isAlive()) { lastFeedback = tr(STR_PET_NO_PET_ERR); return false; }
  if (plot.isSleeping) { lastFeedback = tr(STR_PET_BLOCKED_SLEEPING); return false; }
  if (plot.isSick)     { lastFeedback = tr(STR_PET_BLOCKED_EXERCISE); return false; }

  unsigned long now = millis();
  if (now - lastExerciseMs < PetConfig::EXERCISE_COOLDOWN_MS) {
    lastFeedback = tr(STR_PET_BLOCKED_COOLDOWN);
    return false;
  }

  lastExerciseMs = now;
  plot.weight    = clampSub(plot.weight, PetConfig::WEIGHT_PER_EXERCISE);
  plot.happiness = clampAdd(plot.happiness, 10);

  lastFeedback = tr(STR_PET_EXERCISED);
  LOG_DBG("PET", "exercise: weight=%d", plot.weight);
  save();
  return true;
}

bool PetManager::cleanBathroom() {
  PetState& plot = activePlot();
  if (!plot.exists() || !plot.isAlive()) { lastFeedback = tr(STR_PET_NO_PET_ERR); return false; }
  if (plot.wasteCount == 0) { lastFeedback = tr(STR_PET_ALREADY_CLEAN); return false; }

  lastFeedback = tr(STR_PET_CLEANED);
  LOG_DBG("PET", "cleanBathroom: removed %d waste piles", plot.wasteCount);
  plot.wasteCount = 0;
  save();
  return true;
}

bool PetManager::disciplinePet() {
  PetState& plot = activePlot();
  if (!plot.exists() || !plot.isAlive()) { lastFeedback = tr(STR_PET_NO_PET_ERR); return false; }
  if (plot.isSleeping) { lastFeedback = tr(STR_PET_BLOCKED_SLEEPING); return false; }

  if (farm.fertilizerStock == 0) {
    lastFeedback = tr(STR_PET_NO_FERTILIZER);
    return false;
  }
  farm.fertilizerStock--;

  plot.discipline = clampAdd(plot.discipline, 25);

  if (plot.attentionCall && plot.currentNeed == PetNeed::NONE) {
    plot.attentionCall = false;
  }

  lastFeedback = tr(STR_PET_SCOLDED_GOOD);
  save();
  return true;
}

bool PetManager::ignoreCry() {
  PetState& plot = activePlot();
  if (!plot.exists() || !plot.isAlive()) { lastFeedback = tr(STR_PET_NO_PET_ERR); return false; }
  if (!plot.attentionCall) { lastFeedback = tr(STR_PET_NOTHING_IGNORE); return false; }

  if (plot.isFakeCall) {
    // Ignoring fake cry = good training
    plot.discipline = clampAdd(plot.discipline, PetConfig::DISCIPLINE_PER_IGNORE_FAKE);
    lastFeedback = tr(STR_PET_IGNORED_GOOD);
  } else {
    // Ignoring a real need = neglect
    if (plot.careMistakes < 255) plot.careMistakes++;
    plot.happiness = clampSub(plot.happiness, 10);
    lastFeedback = tr(STR_PET_IGNORED_BAD);
  }

  plot.attentionCall = false;
  plot.isFakeCall = false;
  plot.currentNeed = PetNeed::NONE;
  save();
  return true;
}

bool PetManager::toggleLights() {
  PetState& plot = activePlot();
  if (!plot.exists() || !plot.isAlive()) { lastFeedback = tr(STR_PET_NO_PET_ERR); return false; }

  plot.lightsOff = plot.lightsOff ? 0 : 1;
  lastFeedback = plot.lightsOff ? tr(STR_PET_LIGHTS_OFF) : tr(STR_PET_LIGHTS_ON);
  save();
  return true;
}
