#include "PetDecayEngine.h"

#include <Arduino.h>
#include <Logging.h>

namespace {

// Returns true if the given hour-of-day falls within pet sleep window
static inline bool isSleepHour(uint8_t hour) {
  return hour >= PetConfig::SLEEP_HOUR || hour < PetConfig::WAKE_HOUR;
}

static inline uint8_t clampSub(uint8_t val, uint8_t amount) {
  return (val > amount) ? (val - amount) : 0;
}

static inline uint8_t clampAdd(uint8_t val, uint8_t amount) {
  uint16_t r = static_cast<uint16_t>(val) + amount;
  return (r > PetConfig::MAX_STAT) ? PetConfig::MAX_STAT : static_cast<uint8_t>(r);
}

// Apply one simulated hour of decay (called per hour in the loop)
static void applyOneHour(PetState& state, const PetFarmState& farm, uint8_t currentHour, uint32_t h) {
  // Skip most decay while sleeping (only light effects apply)
  if (isSleepHour(currentHour)) {
    state.isSleeping = true;
    // Lights left on during sleep: mild happiness drain
    if (state.lightsOff == 0) {
      if (h % 2 == 0) state.happiness = clampSub(state.happiness, 1);
    }
    return;
  }
  state.isSleeping = false;

  // Hunger (Moisture) decays every hour while awake, halved if Self-Watering Pot is equipped
  uint8_t hungerDecay = PetConfig::HUNGER_DECAY_PER_HOUR;
  if (farm.equipSelfWateringPot) {
    if (h % 2 != 0) hungerDecay = 0;
  }
  state.hunger = clampSub(state.hunger, hungerDecay);

  // Slow-Release Fertilizer passive: regenerates nutrients (+1 discipline every 2 hours)
  if (farm.equipSlowReleaseFertilizer && h % 2 == 0) {
    state.discipline = clampAdd(state.discipline, 1);
  }

  // Happiness (Sunlight) decays every ~2 hours
  if (h % 2 == 0) {
    uint8_t decay = PetConfig::HAPPINESS_DECAY_PER_HOUR;
    // Sickness doubles happiness decay
    if (state.isSick) decay += PetConfig::SICK_HAPPINESS_PENALTY;
    // Each waste pile drains happiness
    if (state.wasteCount > 0) decay += state.wasteCount * PetConfig::WASTE_HAPPINESS_PENALTY;

    // Moss Pole halves the happiness decay!
    if (farm.equipMossPole) {
      if (decay > 1) {
        decay /= 2;
      } else if (decay == 1 && h % 4 == 0) {
        decay = 0;
      }
    }
    state.happiness = clampSub(state.happiness, decay);
  }

  // Health drains when starving
  if (state.hunger == 0) {
    uint8_t healthDecay = PetConfig::HEALTH_DECAY_PER_HOUR;
    if (farm.equipGreenhouseCover) {
      healthDecay /= 2;
    }
    state.health = clampSub(state.health, healthDecay);
  }
  // Underweight also drains health slowly
  if (state.weight < PetConfig::UNDERWEIGHT_THRESHOLD && h % 4 == 0) {
    uint8_t healthDecay = 1;
    if (farm.equipGreenhouseCover && h % 8 != 0) {
      healthDecay = 0;
    }
    state.health = clampSub(state.health, healthDecay);
  }

  // Weight trends toward normal (1 point per 12 awake hours)
  if (h % 12 == 0) {
    if (state.weight > PetConfig::NORMAL_WEIGHT) {
      state.weight = clampSub(state.weight, 1);
    } else if (state.weight < PetConfig::NORMAL_WEIGHT) {
      state.weight = clampAdd(state.weight, 1);
    }
  }

  // Sickness auto-recovery counter
  if (state.isSick) {
    if (state.sicknessTimer > 0) {
      state.sicknessTimer--;
    } else {
      // Auto-recover after SICK_RECOVERY_HOURS
      state.isSick = false;
      LOG_DBG("PET", "Pet auto-recovered from sickness");
    }
  }

  // Trigger sickness from overweight (10% chance per hour while overweight)
  if (!state.isSick && state.weight > PetConfig::OVERWEIGHT_THRESHOLD) {
    if (random(10) == 0) {
      state.isSick = true;
      state.sicknessTimer = PetConfig::SICK_RECOVERY_HOURS;
      LOG_DBG("PET", "Pet got sick from being overweight");
    }
  }
}

}  // namespace

namespace PetDecayEngine {

void applyDecay(PetState& state, const PetFarmState& farm, uint32_t elapsedHours, uint8_t startHour) {
  // Cap to prevent overflow from very long absences (max 30 days = 720h)
  if (elapsedHours > 720) elapsedHours = 720;

  for (uint32_t h = 0; h < elapsedHours; h++) {
    uint8_t currentHour = static_cast<uint8_t>((startHour + h) % 24);
    applyOneHour(state, farm, currentHour, h);

    // Death check after each hour
    if (state.health == 0) {
      state.stage = PetStage::DEAD;
      LOG_DBG("PET", "Pet died after %lu hours of decay", (unsigned long)h);
      return;
    }
  }
}

}  // namespace PetDecayEngine
