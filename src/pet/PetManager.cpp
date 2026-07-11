#include "PetManager.h"

#include "PetCareTracker.h"
#include "PetDecayEngine.h"
#include "PetEvolution.h"
#include "CrossPointSettings.h"

#include <Arduino.h>
#include <I18n.h>
#include <Logging.h>

#include "activities/reader/GlobalReadingStats.h"
#include "activities/reader/ReadingStatsUtils.h"

#include <algorithm>
#include <cstring>
#include <ctime>

PetManager& PetManager::getInstance() {
  static PetManager instance;
  return instance;
}

// --- Game Logic ---

void PetManager::tick() {
  if (!state.exists() || !state.isAlive()) return;
  if (!isTimeValid()) return;

  uint32_t now = getCurrentTime();
  if (state.lastTickTime == 0) {
    state.lastTickTime = now;
    save();
    return;
  }
  if (now <= state.lastTickTime) return;

  uint32_t elapsedSec = now - state.lastTickTime;
  uint32_t elapsedHours = elapsedSec / 3600;
  if (elapsedHours == 0) return;

  // Determine the hour-of-day at the start of the elapsed period for sleep simulation
  struct tm startTm;
  time_t startTime = static_cast<time_t>(state.lastTickTime);
  time_t localStartTime = startTime + (static_cast<int>(SETTINGS.clockUtcOffsetQ) - 48) * 900;
  gmtime_r(&localStartTime, &startTm);
  uint8_t startHour = static_cast<uint8_t>(startTm.tm_hour);

  PetDecayEngine::applyDecay(state, elapsedHours, startHour);
  PetCareTracker::checkCareMistakes(state, elapsedHours);
  PetCareTracker::expireAttentionCall(state, now);
  PetCareTracker::generateAttentionCall(state, now);

  state.lastTickTime = now;

  uint8_t elapsedDays = static_cast<uint8_t>(elapsedHours / 24);
  if (elapsedDays > 0) {
    state.daysAtStage += elapsedDays;
    state.totalAge    += elapsedDays;
    PetCareTracker::updateCareScore(state);
    PetEvolution::checkEvolution(state);
    updateStreak();

    // Elder lifespan death check
    if (state.stage == PetStage::ELDER && state.isAlive()) {
      uint16_t lifespan = (state.careMistakes < PetConfig::ELDER_LIFESPAN_DAYS)
                              ? PetConfig::ELDER_LIFESPAN_DAYS - state.careMistakes
                              : 1;
      if (state.totalAge >= lifespan) {
        state.stage = PetStage::DEAD;
        LOG_DBG("PET", "Pet died of old age");
      }
    }
  }

  save();
}


void PetManager::updateStreak() {
  uint16_t today = getDayOfYear();
  if (today == 0) return;
  if (state.lastReadDay > 0) {
    uint16_t diff = (today >= state.lastReadDay) ? (today - state.lastReadDay) : 1;
    if (diff > 1) state.currentStreak = 0;
  }
}

void PetManager::resetMissionsIfNewDay() {
  uint16_t today = getDayOfYear();
  if (today > 0 && today != state.missionDay) {
    state.missionDay = today;
    state.missionPagesRead = 0;
    state.missionPetCount = 0;
    state.missionWaterCount = 0;
    state.missionPruneCount = 0;
    state.missionWeedCount = 0;
    state.missionFertilizerCount = 0;
    state.questReadClaimed = false;
    state.questPetClaimed = false;
    state.questWaterClaimed = false;
    state.questPruneClaimed = false;
    state.questWeedClaimed = false;
    state.questFertilizerClaimed = false;
  }
}

// Applies `pages` worth of meals (PetConfig::PAGES_PER_MEAL pages = 1 meal) to
// hunger/weight/waste, shared by both the lazy reading-stats sync and the
// real-time page-turn hook.
void PetManager::feedFromPages(uint32_t pages) {
  state.pageAccumulator = static_cast<uint16_t>(
      std::min<uint32_t>(state.pageAccumulator + pages, 60000));

  uint32_t meals = state.pageAccumulator / PetConfig::PAGES_PER_MEAL;
  if (meals == 0) return;
  if (meals > 50) meals = 50;  // cap to prevent stat overflow
  state.pageAccumulator = static_cast<uint16_t>(state.pageAccumulator % PetConfig::PAGES_PER_MEAL);

  for (uint32_t i = 0; i < meals; i++) {
    state.hunger = clampAdd(state.hunger, PetConfig::HUNGER_PER_MEAL);
    state.weight = clampAdd(state.weight, PetConfig::WEIGHT_PER_MEAL);
    state.mealsSinceClean++;
    if (state.mealsSinceClean >= PetConfig::MEALS_UNTIL_WASTE) {
      state.mealsSinceClean = 0;
      if (state.wasteCount < PetConfig::MAX_WASTE) state.wasteCount++;
    }
  }

  if (state.health < PetConfig::MAX_STAT && state.hunger > 0)
    state.health = clampAdd(state.health, 5);
  uint8_t happinessBonus = static_cast<uint8_t>(meals * 3 > 30 ? 30 : meals * 3);
  state.happiness = clampAdd(state.happiness, happinessBonus);
}

// --- Sync from GlobalReadingStats (lazy evaluation) ---

void PetManager::syncFromReadingStats(const GlobalReadingStats& stats) {
  if (!state.exists() || !state.isAlive()) return;

  // First-sync migration guard: don't retroactively apply all historical reading
  if (state.lastKnownReadSeconds == 0 && state.lastKnownPagesTurned == 0 && state.lastTickTime > 0) {
    state.lastKnownReadSeconds = stats.totalReadingSeconds;
    state.lastKnownPagesTurned = stats.totalPagesTurned;
    state.lastUpdateTimestamp = getCurrentTime();
    LOG_DBG("PET", "Migration: initialized lastKnownReadSeconds=%lu lastKnownPagesTurned=%lu",
            (unsigned long)stats.totalReadingSeconds, (unsigned long)stats.totalPagesTurned);
    return;
  }

  // Sync streak and books from GlobalReadingStats
  ReadingStatsDateTime today;
  state.currentStreak = getCurrentLocalReadingStatsDateTime(today) ? stats.currentReadingStreak(&today.date) : 0;
  state.lastReadDay = getDayOfYear();  // keep in sync to prevent tick() from resetting streak
  if (stats.completedBooks > state.booksFinished) {
    uint32_t diff = stats.completedBooks - state.booksFinished;
    state.inkPoints += diff * 100;
  }
  state.booksFinished = (stats.completedBooks > 255) ? 255 : static_cast<uint8_t>(stats.completedBooks);

  state.longestReadingStreak = stats.longestReadingStreak;

  if (state.lastKnownSessions == 0) {
    state.lastKnownSessions = stats.totalSessions;
  } else if (stats.totalSessions > state.lastKnownSessions) {
    uint32_t diffSessions = stats.totalSessions - state.lastKnownSessions;
    state.inkPoints += diffSessions * 5;
    state.lastKnownSessions = stats.totalSessions;
  }

  if (state.lastKnownReadSeconds == 0) {
    state.lastKnownReadSeconds = stats.totalReadingSeconds;
  } else if (stats.totalReadingSeconds > state.lastKnownReadSeconds) {
    uint32_t diffSeconds = stats.totalReadingSeconds - state.lastKnownReadSeconds;
    state.inkPoints += diffSeconds / 30;
    state.lastKnownReadSeconds = stats.totalReadingSeconds;
  }

  // Real page count for evolution thresholds (crossink tracks this precisely,
  // unlike the reading-time estimate the original pet used)
  state.totalPagesRead = stats.totalPagesTurned;

  // Feed from any pages turned since the last sync that onPageTurned() hasn't
  // already accounted for (e.g. pet screen opened without reading in between).
  if (stats.totalPagesTurned > state.lastKnownPagesTurned) {
    feedFromPages(stats.totalPagesTurned - state.lastKnownPagesTurned);
  }

  // Update streak tier
  if (state.currentStreak >= 30)      state.streakTier = 3;
  else if (state.currentStreak >= 14) state.streakTier = 2;
  else if (state.currentStreak >= 7)  state.streakTier = 1;
  else                                state.streakTier = 0;

  state.lastKnownReadSeconds = stats.totalReadingSeconds;
  state.lastKnownPagesTurned = stats.totalPagesTurned;
  state.lastUpdateTimestamp = getCurrentTime();
  LOG_DBG("PET", "Synced: pages=%lu streak=%d happy=%d",
          (unsigned long)state.totalPagesRead, state.currentStreak, state.happiness);
}

void PetManager::onPageTurned() {
  load();
  if (!state.exists() || !state.isAlive()) return;
  updateSleepState();
  if (state.isSleeping || state.isSick) return;

  resetMissionsIfNewDay();
  if (state.missionPagesRead < 30) {
    state.missionPagesRead++;
    if (state.missionPagesRead >= 30 && !state.questReadClaimed) {
      state.inkPoints += 50;
      state.questReadClaimed = true;
    }
  }
  state.totalPagesRead++;
  state.lastKnownPagesTurned++;
  state.inkPoints++;
  feedFromPages(1);
  // Not saved here — per-page-turn SD writes are a debounce violation.
  // EpubReaderActivity::onExit() persists this via PET_MANAGER.save() when
  // the reading session ends.
}

void PetManager::onChapterFinished() {
  load();
  if (!state.exists() || !state.isAlive()) return;
  updateSleepState();
  if (state.isSleeping || state.isSick) return;

  state.inkPoints += 10;
}

// --- User interaction ---

bool PetManager::pet() {
  if (!state.exists() || !state.isAlive()) return false;

  unsigned long now = millis();
  if (now - lastPetTimeMs < PetConfig::PET_COOLDOWN_MS) return false;

  lastPetTimeMs = now;
  state.happiness = clampAdd(state.happiness, PetConfig::HAPPINESS_PER_PET);
  resetMissionsIfNewDay();
  if (state.missionPetCount < 5) {
    state.missionPetCount++;
    if (state.missionPetCount >= 5 && !state.questPetClaimed) {
      state.inkPoints += 30;
      state.questPetClaimed = true;
    }
  }
  LOG_DBG("PET", "Tended! happiness=%d", state.happiness);
  save();
  return true;
}

void PetManager::hatchNew(const char* name, uint8_t type) {
  state = PetState();  // Resets all fields to struct defaults
  state.initialized = true;
  state.stage = PetStage::EGG;
  state.hunger = 80;
  state.happiness = 80;
  state.health = 100;
  state.petType = type;
  if (name && name[0]) {
    strncpy(state.petName, name, sizeof(state.petName) - 1);
    state.petName[sizeof(state.petName) - 1] = '\0';
  }
  if (isTimeValid()) {
    state.birthTime = getCurrentTime();
    state.lastTickTime = state.birthTime;
  }
  save();
  LOG_DBG("PET", "New egg hatched! name='%s' type=%d", state.petName, state.petType);
}

bool PetManager::renamePet(const char* name) {
  if (!state.exists()) return false;
  if (name && name[0]) {
    strncpy(state.petName, name, sizeof(state.petName) - 1);
    state.petName[sizeof(state.petName) - 1] = '\0';
  } else {
    state.petName[0] = '\0';
  }
  return save();
}

bool PetManager::changeType(uint8_t type) {
  if (!state.exists()) return false;
  state.petType = type;
  return save();
}

// --- State queries ---

PetMood PetManager::getMood() const {
  if (!state.isAlive())  return PetMood::DEAD;
  if (state.isSleeping)  return PetMood::SLEEPING;
  if (state.isSick)      return PetMood::SICK;
  if (state.attentionCall) return PetMood::NEEDY;
  // Low discipline → occasional refusal behaviour
  if (state.discipline < 30 && random(100) < 20) return PetMood::REFUSING;
  if (state.hunger > 70 && state.health > 70) return PetMood::HAPPY;
  if (state.hunger > 30 && state.health > 30) return PetMood::NEUTRAL;
  return PetMood::SAD;
}

uint32_t PetManager::getDaysAlive() const {
  if (!state.exists() || state.birthTime == 0 || !isTimeValid()) return 0;
  uint32_t now = getCurrentTime();
  if (now <= state.birthTime) return 0;
  return (now - state.birthTime) / 86400;
}

void PetManager::getMissions(PetMission out[6]) const {
  out[0] = {tr(STR_PET_MISSION_READ), state.missionPagesRead, 30, state.missionPagesRead >= 30, 50};
  out[1] = {tr(STR_PET_MISSION_PET),  state.missionPetCount,   5, state.missionPetCount  >=  5, 30};
  out[2] = {tr(STR_PET_MISSION_WATER), state.missionWaterCount,  3, state.missionWaterCount >=  3, 20};
  out[3] = {tr(STR_PET_MISSION_PRUNE), state.missionPruneCount,  1, state.missionPruneCount >=  1, 30};
  out[4] = {tr(STR_PET_MISSION_WEED),  state.missionWeedCount,   1, state.missionWeedCount  >=  1, 20};
  out[5] = {tr(STR_PET_MISSION_FERTILIZE), state.missionFertilizerCount, 1, state.missionFertilizerCount >= 1, 20};
}

// --- Helpers ---

// Fill `out` with the current local time. Returns false if the clock is
// clearly unset (year < 2025), mirroring Arduino's getLocalTime() validity
// check. Uses portable C time calls (not Arduino's getLocalTime) so this
// builds on both hardware and the simulator.
static bool localTimeNow(struct tm& out) {
  time_t now = time(nullptr);
  time_t localNow = now + (static_cast<int>(SETTINGS.clockUtcOffsetQ) - 48) * 900;
  gmtime_r(&localNow, &out);
  return out.tm_year >= 125;  // tm_year is years since 1900; 125 => 2025
}

bool PetManager::isTimeValid() const {
  struct tm timeinfo;
  return localTimeNow(timeinfo);
}

uint32_t PetManager::getCurrentTime() const {
  return static_cast<uint32_t>(time(nullptr));
}

uint16_t PetManager::getDayOfYear() const {
  struct tm timeinfo;
  if (!localTimeNow(timeinfo)) return 0;
  return static_cast<uint16_t>(timeinfo.tm_yday + 1);
}

void PetManager::updateSleepState() {
  if (!state.exists()) return;

  struct tm timeinfo;
  if (localTimeNow(timeinfo)) {
    uint8_t hour = static_cast<uint8_t>(timeinfo.tm_hour);
    state.isSleeping = (hour >= PetConfig::SLEEP_HOUR || hour < PetConfig::WAKE_HOUR);
  } else {
    state.isSleeping = false;
  }
}

uint8_t PetManager::clampSub(uint8_t val, uint8_t amount) {
  return (val > amount) ? (val - amount) : 0;
}

uint8_t PetManager::clampAdd(uint8_t val, uint8_t amount) {
  uint16_t result = static_cast<uint16_t>(val) + amount;
  return (result > PetConfig::MAX_STAT) ? PetConfig::MAX_STAT : static_cast<uint8_t>(result);
}
