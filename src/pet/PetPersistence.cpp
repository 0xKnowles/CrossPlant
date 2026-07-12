#include "PetManager.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>

#include <cstring>
#include <ctime>

// --- Persistence ---

bool PetManager::load() {
  // PetManager is a RAM-resident singleton for the whole boot session — once
  // loaded, later load() calls from other call sites (reader exit, pet screen
  // open) must not clobber in-memory progress accumulated by onPageTurned().
  if (loaded) return true;

  if (!Storage.exists(PetConfig::STATE_PATH)) {
    LOG_DBG("PET", "No pet state file found");
    loaded = true;
    return true;  // No pet yet — not an error
  }

  String json = Storage.readFile(PetConfig::STATE_PATH);
  if (json.isEmpty()) {
    LOG_ERR("PET", "Failed to read pet state");
    return false;
  }

  JsonDocument doc;
  auto err = deserializeJson(doc, json);
  if (err) {
    LOG_ERR("PET", "JSON parse error: %s", err.c_str());
    return false;
  }

  // Core fields
  state.stage           = static_cast<PetStage>(doc["stage"] | 0);
  state.hunger          = doc["hunger"]          | (uint8_t)80;
  state.happiness       = doc["happiness"]       | (uint8_t)80;
  state.health          = doc["health"]          | (uint8_t)100;
  state.birthTime       = doc["birthTime"]       | (uint32_t)0;
  state.lastTickTime    = doc["lastTickTime"]    | (uint32_t)0;
  state.totalPagesRead  = doc["totalPagesRead"]  | (uint32_t)0;
  state.currentStreak   = doc["currentStreak"]   | (uint16_t)0;
  state.daysAtStage     = doc["daysAtStage"]     | (uint8_t)0;
  state.lastReadDay     = doc["lastReadDay"]      | (uint16_t)0;
  state.pageAccumulator = doc["pageAccumulator"] | (uint16_t)0;
  // Customization fields — backward compat: missing = empty name, default type
  const char* petNameStr = doc["petName"] | "";
  strncpy(state.petName, petNameStr, sizeof(state.petName) - 1);
  state.petName[sizeof(state.petName) - 1] = '\0';
  state.petType = doc["petType"] | (uint8_t)0;

  // Backward compat: infer initialized from birthTime if field missing
  state.initialized     = doc["initialized"]     | (state.birthTime > 0);
  state.missionDay      = doc["missionDay"]      | (uint16_t)0;
  state.missionPagesRead = doc["missionPagesRead"] | (uint8_t)0;
  state.missionPetCount = doc["missionPetCount"] | (uint8_t)0;
  state.missionWaterCount = doc["missionWaterCount"] | (uint8_t)0;
  state.maxSessionPagesToday   = doc["maxSessionPagesToday"] | doc["missionPruneCount"] | (uint8_t)0;
  state.pagesReadAfter9PM      = doc["pagesReadAfter9PM"] | doc["missionWeedCount"] | (uint8_t)0;
  state.questReadClaimed = doc["questReadClaimed"] | false;
  state.questPetClaimed = doc["questPetClaimed"] | false;
  state.questWaterClaimed = doc["questWaterClaimed"] | false;
  state.questSpeedyClaimed = doc["questSpeedyClaimed"] | doc["questPruneClaimed"] | false;
  state.questNightOwlClaimed = doc["questNightOwlClaimed"] | doc["questWeedClaimed"] | false;
  state.questStreakSaverClaimed = doc["questStreakSaverClaimed"] | doc["questFertilizerClaimed"] | false;

  // New fields — backward-compat: missing keys use struct defaults
  state.weight           = doc["weight"]           | (uint8_t)50;
  state.isSick           = doc["isSick"]           | false;
  state.sicknessTimer    = doc["sicknessTimer"]    | (uint8_t)0;
  state.waterStock       = doc["waterStock"]       | (uint8_t)3;
  state.fertilizerStock  = doc["fertilizerStock"]  | (uint8_t)3;
  state.wasteCount       = doc["wasteCount"]       | (uint8_t)0;
  state.mealsSinceClean  = doc["mealsSinceClean"]  | (uint8_t)0;
  state.discipline       = doc["discipline"]       | (uint8_t)50;
  state.attentionCall    = doc["attentionCall"]    | false;
  state.isFakeCall       = doc["isFakeCall"]       | false;
  state.currentNeed      = static_cast<PetNeed>(doc["currentNeed"] | (uint8_t)0);
  state.lastCallTime     = doc["lastCallTime"]     | (uint32_t)0;
  state.isSleeping       = doc["isSleeping"]       | false;
  state.lightsOff        = doc["lightsOff"]        | (uint8_t)0;
  state.totalAge         = doc["totalAge"]         | (uint16_t)0;
  state.careMistakes     = doc["careMistakes"]     | (uint8_t)0;
  state.avgCareScore     = doc["avgCareScore"]     | (uint8_t)50;
  state.evolutionVariant = doc["evolutionVariant"] | (uint8_t)0;
  state.booksFinished    = doc["booksFinished"]    | (uint8_t)0;
  state.streakTier       = doc["streakTier"]       | (uint8_t)0;

  state.inkPoints        = doc["inkPoints"]        | (uint32_t)0;
  state.hasMossPole               = doc["hasMossPole"] | doc["hasGlasses"] | false;
  state.equipMossPole             = doc["equipMossPole"] | doc["equipGlasses"] | false;
  state.hasSelfWateringPot        = doc["hasSelfWateringPot"] | doc["hasHat"] | false;
  state.equipSelfWateringPot      = doc["equipSelfWateringPot"] | doc["equipHat"] | false;
  state.hasSlowReleaseFertilizer  = doc["hasSlowReleaseFertilizer"] | doc["hasCrown"] | false;
  state.equipSlowReleaseFertilizer= doc["equipSlowReleaseFertilizer"] | doc["equipCrown"] | false;
  state.hasGreenhouseCover        = doc["hasGreenhouseCover"] | doc["hasScarf"] | false;
  state.equipGreenhouseCover      = doc["equipGreenhouseCover"] | doc["equipScarf"] | false;
  state.hasPremiumSprayer         = doc["hasPremiumSprayer"] | doc["hasToy"] | false;
  state.longestReadingStreak = doc["longestReadingStreak"] | (uint16_t)0;
  state.lastKnownSessions = doc["lastKnownSessions"] | (uint32_t)0;
  state.unlockedStages    = doc["unlockedStages"] | (uint16_t)0;
  state.weatherCondition = doc["weatherCondition"] | (uint8_t)0;
  state.weatherTemp      = doc["weatherTemp"]      | (int8_t)0;
  state.lastWeatherSync  = doc["lastWeatherSync"]  | (uint32_t)0;

  // Lazy-eval fields
  state.lastKnownReadSeconds = doc["lastKnownReadSeconds"] | (uint32_t)0;
  state.lastKnownPagesTurned = doc["lastKnownPagesTurned"] | (uint32_t)0;
  state.lastUpdateTimestamp  = doc["lastUpdateTimestamp"]   | (uint32_t)0;

  // Restore clock if RTC lost time (power cycle) but SD card has a saved timestamp
  uint32_t savedTime = doc["savedTime"] | (uint32_t)0;
  if (savedTime > 0) {
    time_t now = time(nullptr);
    struct tm check;
    gmtime_r(&now, &check);
    if (check.tm_year < 125) {
      struct timeval tv = {static_cast<time_t>(savedTime), 0};
      settimeofday(&tv, nullptr);
      LOG_DBG("PET", "Restored clock from SD: %lu", (unsigned long)savedTime);
    }
  }

  updateSleepState();
  loaded = true;
  LOG_DBG("PET", "Loaded pet: stage=%d hunger=%d happy=%d health=%d isSleeping=%d",
          (int)state.stage, state.hunger, state.happiness, state.health, state.isSleeping);
  return true;
}

bool PetManager::save() {
  Storage.mkdir(PetConfig::PET_DIR);

  // Auto-unlock current plant species and stage
  if (state.initialized && state.petType < 3) {
    uint8_t st = (uint8_t)state.stage; // PetStage: 0=Egg, 1=Hatchling, 2=Youngster, 3=Companion, 4=Prized
    if (st >= 1 && st <= 4) {
      uint8_t bit = (state.petType * 4) + (st - 1);
      state.unlockedStages |= (1 << bit);
    }
  }

  JsonDocument doc;
  // Customization
  doc["petName"]        = state.petName;
  doc["petType"]        = state.petType;
  // Core fields
  doc["initialized"]    = state.initialized;
  doc["stage"]          = static_cast<uint8_t>(state.stage);
  doc["hunger"]         = state.hunger;
  doc["happiness"]      = state.happiness;
  doc["health"]         = state.health;
  doc["birthTime"]      = state.birthTime;
  doc["lastTickTime"]   = state.lastTickTime;
  doc["totalPagesRead"] = state.totalPagesRead;
  doc["currentStreak"]  = state.currentStreak;
  doc["daysAtStage"]    = state.daysAtStage;
  doc["lastReadDay"]    = state.lastReadDay;
  doc["pageAccumulator"] = state.pageAccumulator;
  doc["missionDay"]     = state.missionDay;
  doc["missionPagesRead"] = state.missionPagesRead;
  doc["missionPetCount"]  = state.missionPetCount;
  doc["missionWaterCount"] = state.missionWaterCount;
  doc["maxSessionPagesToday"]   = state.maxSessionPagesToday;
  doc["pagesReadAfter9PM"]      = state.pagesReadAfter9PM;
  doc["questReadClaimed"] = state.questReadClaimed;
  doc["questPetClaimed"] = state.questPetClaimed;
  doc["questWaterClaimed"] = state.questWaterClaimed;
  doc["questSpeedyClaimed"] = state.questSpeedyClaimed;
  doc["questNightOwlClaimed"] = state.questNightOwlClaimed;
  doc["questStreakSaverClaimed"] = state.questStreakSaverClaimed;

  // New fields
  doc["weight"]           = state.weight;
  doc["isSick"]           = state.isSick;
  doc["sicknessTimer"]    = state.sicknessTimer;
  doc["waterStock"]       = state.waterStock;
  doc["fertilizerStock"]  = state.fertilizerStock;
  doc["wasteCount"]       = state.wasteCount;
  doc["mealsSinceClean"]  = state.mealsSinceClean;
  doc["discipline"]       = state.discipline;
  doc["attentionCall"]    = state.attentionCall;
  doc["isFakeCall"]       = state.isFakeCall;
  doc["currentNeed"]      = static_cast<uint8_t>(state.currentNeed);
  doc["lastCallTime"]     = state.lastCallTime;
  doc["isSleeping"]       = state.isSleeping;
  doc["lightsOff"]        = state.lightsOff;
  doc["totalAge"]         = state.totalAge;
  doc["careMistakes"]     = state.careMistakes;
  doc["avgCareScore"]     = state.avgCareScore;
  doc["evolutionVariant"] = state.evolutionVariant;
  doc["booksFinished"]    = state.booksFinished;
  doc["streakTier"]       = state.streakTier;

  doc["inkPoints"]        = state.inkPoints;
  doc["hasMossPole"]               = state.hasMossPole;
  doc["equipMossPole"]             = state.equipMossPole;
  doc["hasSelfWateringPot"]        = state.hasSelfWateringPot;
  doc["equipSelfWateringPot"]      = state.equipSelfWateringPot;
  doc["hasSlowReleaseFertilizer"]  = state.hasSlowReleaseFertilizer;
  doc["equipSlowReleaseFertilizer"]= state.equipSlowReleaseFertilizer;
  doc["hasGreenhouseCover"]        = state.hasGreenhouseCover;
  doc["equipGreenhouseCover"]      = state.equipGreenhouseCover;
  doc["hasPremiumSprayer"]         = state.hasPremiumSprayer;
  doc["longestReadingStreak"] = state.longestReadingStreak;
  doc["lastKnownSessions"] = state.lastKnownSessions;
  doc["unlockedStages"]    = state.unlockedStages;
  doc["weatherCondition"] = state.weatherCondition;
  doc["weatherTemp"]      = state.weatherTemp;
  doc["lastWeatherSync"]  = state.lastWeatherSync;

  // Lazy-eval fields
  doc["lastKnownReadSeconds"] = state.lastKnownReadSeconds;
  doc["lastKnownPagesTurned"] = state.lastKnownPagesTurned;
  doc["lastUpdateTimestamp"]  = state.lastUpdateTimestamp;

  // Persist current timestamp for clock restoration after power cycle
  doc["savedTime"] = (uint32_t)time(nullptr);

  String json;
  serializeJson(doc, json);
  bool ok = Storage.writeFile(PetConfig::STATE_PATH, json);
  if (ok) {
    LOG_DBG("PET", "State saved");
  } else {
    LOG_ERR("PET", "Failed to save state");
  }
  return ok;
}
