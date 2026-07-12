#include "PetManager.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>

#include <cstring>
#include <ctime>

namespace {

// Reads one plot's fields from `obj` (either the legacy flat document root,
// or one element of the new "plots" array) into `plot`. Every key uses the
// exact same name the single-pet schema always used, so the legacy-flat and
// new-nested readers share this one function — only the JSON node they're
// called with differs.
void loadPlotFields(PetState& plot, JsonObjectConst obj) {
  plot.stage           = static_cast<PetStage>(obj["stage"] | 0);
  plot.hunger          = obj["hunger"]          | (uint8_t)80;
  plot.happiness       = obj["happiness"]       | (uint8_t)80;
  plot.health           = obj["health"]          | (uint8_t)100;
  plot.birthTime        = obj["birthTime"]       | (uint32_t)0;
  plot.lastTickTime     = obj["lastTickTime"]    | (uint32_t)0;
  plot.totalPagesRead   = obj["totalPagesRead"]  | (uint32_t)0;
  plot.daysAtStage      = obj["daysAtStage"]     | (uint8_t)0;
  plot.pageAccumulator  = obj["pageAccumulator"] | (uint16_t)0;
  const char* petNameStr = obj["petName"] | "";
  strncpy(plot.petName, petNameStr, sizeof(plot.petName) - 1);
  plot.petName[sizeof(plot.petName) - 1] = '\0';
  plot.petType = obj["petType"] | (uint8_t)0;

  // Backward compat: infer initialized from birthTime if field missing
  plot.initialized = obj["initialized"] | (plot.birthTime > 0);

  plot.weight           = obj["weight"]           | (uint8_t)50;
  plot.isSick           = obj["isSick"]           | false;
  plot.sicknessTimer    = obj["sicknessTimer"]    | (uint8_t)0;
  plot.wasteCount       = obj["wasteCount"]       | (uint8_t)0;
  plot.mealsSinceClean  = obj["mealsSinceClean"]  | (uint8_t)0;
  plot.discipline       = obj["discipline"]       | (uint8_t)50;
  plot.attentionCall    = obj["attentionCall"]    | false;
  plot.isFakeCall       = obj["isFakeCall"]       | false;
  plot.currentNeed      = static_cast<PetNeed>(obj["currentNeed"] | (uint8_t)0);
  plot.lastCallTime     = obj["lastCallTime"]     | (uint32_t)0;
  plot.isSleeping       = obj["isSleeping"]       | false;
  plot.lightsOff        = obj["lightsOff"]        | (uint8_t)0;
  plot.totalAge         = obj["totalAge"]         | (uint16_t)0;
  plot.careMistakes     = obj["careMistakes"]     | (uint8_t)0;
  plot.avgCareScore     = obj["avgCareScore"]     | (uint8_t)50;
  plot.evolutionVariant = obj["evolutionVariant"] | (uint8_t)0;
}

// Mirrors loadPlotFields(): writes one plot's fields into a "plots" array element.
void savePlotFields(const PetState& plot, JsonObject obj) {
  obj["petName"]        = plot.petName;
  obj["petType"]        = plot.petType;
  obj["initialized"]    = plot.initialized;
  obj["stage"]          = static_cast<uint8_t>(plot.stage);
  obj["hunger"]         = plot.hunger;
  obj["happiness"]      = plot.happiness;
  obj["health"]         = plot.health;
  obj["birthTime"]      = plot.birthTime;
  obj["lastTickTime"]   = plot.lastTickTime;
  obj["totalPagesRead"] = plot.totalPagesRead;
  obj["daysAtStage"]    = plot.daysAtStage;
  obj["pageAccumulator"] = plot.pageAccumulator;

  obj["weight"]           = plot.weight;
  obj["isSick"]           = plot.isSick;
  obj["sicknessTimer"]    = plot.sicknessTimer;
  obj["wasteCount"]       = plot.wasteCount;
  obj["mealsSinceClean"]  = plot.mealsSinceClean;
  obj["discipline"]       = plot.discipline;
  obj["attentionCall"]    = plot.attentionCall;
  obj["isFakeCall"]       = plot.isFakeCall;
  obj["currentNeed"]      = static_cast<uint8_t>(plot.currentNeed);
  obj["lastCallTime"]     = plot.lastCallTime;
  obj["isSleeping"]       = plot.isSleeping;
  obj["lightsOff"]        = plot.lightsOff;
  obj["totalAge"]         = plot.totalAge;
  obj["careMistakes"]     = plot.careMistakes;
  obj["avgCareScore"]     = plot.avgCareScore;
  obj["evolutionVariant"] = plot.evolutionVariant;
}

}  // namespace

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

  // --- Farm (account-wide) fields — same top-level keys the single-pet
  // schema always used, so legacy saves need no migration for these. ---
  farm.missionDay      = doc["missionDay"]      | (uint16_t)0;
  farm.missionPagesRead = doc["missionPagesRead"] | (uint8_t)0;
  farm.missionPetCount = doc["missionPetCount"] | (uint8_t)0;
  farm.missionWaterCount = doc["missionWaterCount"] | (uint8_t)0;
  farm.maxSessionPagesToday   = doc["maxSessionPagesToday"] | doc["missionPruneCount"] | (uint8_t)0;
  farm.pagesReadAfter9PM      = doc["pagesReadAfter9PM"] | doc["missionWeedCount"] | (uint8_t)0;
  farm.questReadClaimed = doc["questReadClaimed"] | false;
  farm.questPetClaimed = doc["questPetClaimed"] | false;
  farm.questWaterClaimed = doc["questWaterClaimed"] | false;
  farm.questSpeedyClaimed = doc["questSpeedyClaimed"] | doc["questPruneClaimed"] | false;
  farm.questNightOwlClaimed = doc["questNightOwlClaimed"] | doc["questWeedClaimed"] | false;
  farm.questStreakSaverClaimed = doc["questStreakSaverClaimed"] | doc["questFertilizerClaimed"] | false;

  farm.currentStreak   = doc["currentStreak"]   | (uint16_t)0;
  farm.lastReadDay     = doc["lastReadDay"]      | (uint16_t)0;
  farm.waterStock       = doc["waterStock"]       | (uint8_t)3;
  farm.fertilizerStock  = doc["fertilizerStock"]  | (uint8_t)3;
  farm.booksFinished    = doc["booksFinished"]    | (uint8_t)0;
  farm.streakTier       = doc["streakTier"]       | (uint8_t)0;

  farm.inkPoints        = doc["inkPoints"]        | (uint32_t)0;
  farm.hasMossPole               = doc["hasMossPole"] | doc["hasGlasses"] | false;
  farm.equipMossPole             = doc["equipMossPole"] | doc["equipGlasses"] | false;
  farm.hasSelfWateringPot        = doc["hasSelfWateringPot"] | doc["hasHat"] | false;
  farm.equipSelfWateringPot      = doc["equipSelfWateringPot"] | doc["equipHat"] | false;
  farm.hasSlowReleaseFertilizer  = doc["hasSlowReleaseFertilizer"] | doc["hasCrown"] | false;
  farm.equipSlowReleaseFertilizer= doc["equipSlowReleaseFertilizer"] | doc["equipCrown"] | false;
  farm.hasGreenhouseCover        = doc["hasGreenhouseCover"] | doc["hasScarf"] | false;
  farm.equipGreenhouseCover      = doc["equipGreenhouseCover"] | doc["equipScarf"] | false;
  farm.hasPremiumSprayer         = doc["hasPremiumSprayer"] | doc["hasToy"] | false;
  farm.longestReadingStreak = doc["longestReadingStreak"] | (uint16_t)0;
  farm.lastKnownSessions = doc["lastKnownSessions"] | (uint32_t)0;
  farm.unlockedStages    = doc["unlockedStages"] | (uint16_t)0;
  farm.weatherCondition = doc["weatherCondition"] | (uint8_t)0;
  farm.weatherTemp      = doc["weatherTemp"]      | (int8_t)0;
  farm.lastWeatherSync  = doc["lastWeatherSync"]  | (uint32_t)0;

  farm.lastKnownReadSeconds = doc["lastKnownReadSeconds"] | (uint32_t)0;
  farm.lastKnownPagesTurned = doc["lastKnownPagesTurned"] | (uint32_t)0;
  farm.lastUpdateTimestamp  = doc["lastUpdateTimestamp"]   | (uint32_t)0;

  // --- Growing plots ---
  JsonVariantConst plotsVar = doc["plots"];
  if (!plotsVar.isNull() && plotsVar.is<JsonArrayConst>()) {
    // New nested schema.
    farm.ownedPlotCount  = doc["ownedPlotCount"]  | (uint8_t)1;
    farm.activePlotIndex = doc["activePlotIndex"] | (uint8_t)0;
    if (farm.ownedPlotCount > PetConfig::MAX_PLOTS) farm.ownedPlotCount = PetConfig::MAX_PLOTS;
    if (farm.ownedPlotCount == 0) farm.ownedPlotCount = 1;
    if (farm.activePlotIndex >= farm.ownedPlotCount) farm.activePlotIndex = 0;

    JsonArrayConst plotsArr = plotsVar.as<JsonArrayConst>();
    int i = 0;
    for (JsonObjectConst plotObj : plotsArr) {
      if (i >= PetConfig::MAX_PLOTS) break;
      loadPlotFields(plots[i], plotObj);
      i++;
    }
  } else {
    // Legacy single-plant save: every plot key lived flat at the document
    // root. Load it straight into plots[0] and start with one owned plot.
    loadPlotFields(plots[0], doc.as<JsonObjectConst>());
    farm.ownedPlotCount = 1;
    farm.activePlotIndex = 0;
  }

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
  LOG_DBG("PET", "Loaded pet farm: plots=%d active=%d", farm.ownedPlotCount, farm.activePlotIndex);
  return true;
}

bool PetManager::save() {
  Storage.mkdir(PetConfig::PET_DIR);

  // Auto-unlock every owned plot's current species+stage in the herbarium.
  for (int i = 0; i < farm.ownedPlotCount; i++) {
    const PetState& plot = plots[i];
    if (plot.initialized && plot.petType < 3) {
      uint8_t st = (uint8_t)plot.stage; // PetStage: 0=Egg, 1=Hatchling, 2=Youngster, 3=Companion, 4=Prized
      if (st >= 1 && st <= 4) {
        uint8_t bit = (plot.petType * 4) + (st - 1);
        farm.unlockedStages |= (1 << bit);
      }
    }
  }

  JsonDocument doc;

  doc["missionDay"]     = farm.missionDay;
  doc["missionPagesRead"] = farm.missionPagesRead;
  doc["missionPetCount"]  = farm.missionPetCount;
  doc["missionWaterCount"] = farm.missionWaterCount;
  doc["maxSessionPagesToday"]   = farm.maxSessionPagesToday;
  doc["pagesReadAfter9PM"]      = farm.pagesReadAfter9PM;
  doc["questReadClaimed"] = farm.questReadClaimed;
  doc["questPetClaimed"] = farm.questPetClaimed;
  doc["questWaterClaimed"] = farm.questWaterClaimed;
  doc["questSpeedyClaimed"] = farm.questSpeedyClaimed;
  doc["questNightOwlClaimed"] = farm.questNightOwlClaimed;
  doc["questStreakSaverClaimed"] = farm.questStreakSaverClaimed;

  doc["currentStreak"]  = farm.currentStreak;
  doc["lastReadDay"]    = farm.lastReadDay;
  doc["waterStock"]       = farm.waterStock;
  doc["fertilizerStock"]  = farm.fertilizerStock;
  doc["booksFinished"]    = farm.booksFinished;
  doc["streakTier"]       = farm.streakTier;

  doc["inkPoints"]        = farm.inkPoints;
  doc["hasMossPole"]               = farm.hasMossPole;
  doc["equipMossPole"]             = farm.equipMossPole;
  doc["hasSelfWateringPot"]        = farm.hasSelfWateringPot;
  doc["equipSelfWateringPot"]      = farm.equipSelfWateringPot;
  doc["hasSlowReleaseFertilizer"]  = farm.hasSlowReleaseFertilizer;
  doc["equipSlowReleaseFertilizer"]= farm.equipSlowReleaseFertilizer;
  doc["hasGreenhouseCover"]        = farm.hasGreenhouseCover;
  doc["equipGreenhouseCover"]      = farm.equipGreenhouseCover;
  doc["hasPremiumSprayer"]         = farm.hasPremiumSprayer;
  doc["longestReadingStreak"] = farm.longestReadingStreak;
  doc["lastKnownSessions"] = farm.lastKnownSessions;
  doc["unlockedStages"]    = farm.unlockedStages;
  doc["weatherCondition"] = farm.weatherCondition;
  doc["weatherTemp"]      = farm.weatherTemp;
  doc["lastWeatherSync"]  = farm.lastWeatherSync;

  doc["lastKnownReadSeconds"] = farm.lastKnownReadSeconds;
  doc["lastKnownPagesTurned"] = farm.lastKnownPagesTurned;
  doc["lastUpdateTimestamp"]  = farm.lastUpdateTimestamp;

  doc["ownedPlotCount"]  = farm.ownedPlotCount;
  doc["activePlotIndex"] = farm.activePlotIndex;

  JsonArray plotsArr = doc["plots"].to<JsonArray>();
  for (int i = 0; i < farm.ownedPlotCount; i++) {
    JsonObject plotObj = plotsArr.add<JsonObject>();
    savePlotFields(plots[i], plotObj);
  }

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
