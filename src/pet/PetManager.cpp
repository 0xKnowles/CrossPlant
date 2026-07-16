#include "PetManager.h"

#include "PetCareTracker.h"
#include "PetDecayEngine.h"
#include "PetEvolution.h"
#include "CrossPointSettings.h"
#include <HalStorage.h>

#ifndef SIMULATOR
#include <WiFi.h>
#include <ArduinoJson.h>
#include "network/HttpDownloader.h"
#endif

#include <Arduino.h>
#include <I18n.h>
#include <Logging.h>

#include "activities/reader/GlobalReadingStats.h"
#include "activities/reader/ReadingStatsUtils.h"

#include <algorithm>
#include <cstring>
#include <ctime>

static bool localTimeNow(struct tm& out);

PetManager& PetManager::getInstance() {
  static PetManager instance;
  return instance;
}

namespace {
// True if at least one owned plot is a living, hatched plant. Used to gate
// farm-level bookkeeping (quests, currency, weather sync) so it stays inert
// for accounts that haven't hatched anything yet — same guard the single-plot
// code used to apply directly to `state`.
bool anyPlotExists(const PetState plots[], const PetFarmState& farm) {
  for (int i = 0; i < farm.ownedPlotCount; i++) {
    if (plots[i].exists()) return true;
  }
  return false;
}
}  // namespace

// --- Game Logic ---

void PetManager::tick() {
  if (!anyPlotExists(plots, farm)) return;
  if (!isTimeValid()) return;

  uint32_t now = getCurrentTime();

#ifndef SIMULATOR
  if (WiFi.status() == WL_CONNECTED && (now - farm.lastWeatherSync > 14400)) {
    farm.lastWeatherSync = now;
    save();
    xTaskCreate([](void* param) {
      LOG_INF("PET", "Starting background weather sync task...");
      bool success = false;
      std::string ipJson;
      if (HttpDownloader::fetchUrl("http://ip-api.com/json/", ipJson)) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, ipJson);
        if (!error) {
          double lat = doc["lat"] | 0.0;
          double lon = doc["lon"] | 0.0;
          if (lat != 0.0 || lon != 0.0) {
            char url[128];
            snprintf(url, sizeof(url), "http://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&current_weather=true", lat, lon);
            std::string weatherJson;
            if (HttpDownloader::fetchUrl(url, weatherJson)) {
              JsonDocument wdoc;
              error = deserializeJson(wdoc, weatherJson);
              if (!error) {
                int weathercode = wdoc["current_weather"]["weathercode"] | -1;
                float temp = wdoc["current_weather"]["temperature"] | 0.0f;

                uint8_t cond = 3; // default Cloudy
                if (weathercode >= 0 && weathercode <= 3) {
                  cond = 1; // Sunny / Clear
                } else if ((weathercode >= 51 && weathercode <= 67) || (weathercode >= 80 && weathercode <= 82)) {
                  cond = 2; // Rainy
                } else if ((weathercode >= 71 && weathercode <= 77) || weathercode == 85 || weathercode == 86) {
                  cond = 4; // Snowy
                }

                PET_MANAGER.updateWeather(cond, (int)temp);
                PET_MANAGER.setFeedback(tr(STR_PET_WEATHER_SYNCED));
                LOG_INF("PET", "Weather synced: condition=%d temp=%d", cond, (int)temp);
                success = true;
              }
            }
          }
        }
      }
      if (!success) {
        PET_MANAGER.setFeedback(tr(STR_PET_WEATHER_FAILED));
      }
      vTaskDelete(nullptr);
    }, "weather_task", 4096, nullptr, 1, nullptr);
  }
#endif

  for (int i = 0; i < farm.ownedPlotCount; i++) {
    PetState& plot = plots[i];
    if (!plot.exists() || !plot.isAlive()) continue;

    if (plot.lastTickTime == 0) {
      plot.lastTickTime = now;
      continue;
    }
    if (now <= plot.lastTickTime) continue;

    uint32_t elapsedSec = now - plot.lastTickTime;
    uint32_t elapsedHours = elapsedSec / 3600;
    if (elapsedHours == 0) continue;

    // Determine the hour-of-day at the start of the elapsed period for sleep simulation
    struct tm startTm;
    time_t startTime = static_cast<time_t>(plot.lastTickTime);
    time_t localStartTime = startTime + (static_cast<int>(SETTINGS.clockUtcOffsetQ) - 48) * 900;
    gmtime_r(&localStartTime, &startTm);
    uint8_t startHour = static_cast<uint8_t>(startTm.tm_hour);

    PetDecayEngine::applyDecay(plot, farm, elapsedHours, startHour);
    PetCareTracker::checkCareMistakes(plot, elapsedHours);
    PetCareTracker::expireAttentionCall(plot, now);
    PetCareTracker::generateAttentionCall(plot, now);

    plot.lastTickTime = now;

    uint8_t elapsedDays = static_cast<uint8_t>(elapsedHours / 24);
    if (elapsedDays > 0) {
      plot.daysAtStage += elapsedDays;
      plot.totalAge    += elapsedDays;
      PetCareTracker::updateCareScore(plot);
      PetEvolution::checkEvolution(plot, farm.currentStreak, farm.booksFinished);

      // Elder lifespan death check
      if (plot.stage == PetStage::ELDER && plot.isAlive()) {
        uint16_t lifespan = (plot.careMistakes < PetConfig::ELDER_LIFESPAN_DAYS)
                                ? PetConfig::ELDER_LIFESPAN_DAYS - plot.careMistakes
                                : 1;
        if (plot.totalAge >= lifespan) {
          plot.stage = PetStage::DEAD;
          LOG_DBG("PET", "Plot %d died of old age", i);
        }
      }
    }
  }

  updateStreak();
  save();
}


void PetManager::updateStreak() {
  uint16_t today = getDayOfYear();
  if (today == 0) return;
  if (farm.lastReadDay > 0) {
    uint16_t diff = (today >= farm.lastReadDay) ? (today - farm.lastReadDay) : 1;
    if (diff > 1) farm.currentStreak = 0;
  }
}

void PetManager::resetMissionsIfNewDay() {
  uint16_t today = getDayOfYear();
  if (today > 0 && today != farm.missionDay) {
    farm.missionDay = today;
    farm.missionPagesRead = 0;
    farm.missionPetCount = 0;
    farm.missionWaterCount = 0;
    farm.maxSessionPagesToday = 0;
    farm.pagesReadAfter9PM = 0;
    farm.questReadClaimed = false;
    farm.questPetClaimed = false;
    farm.questWaterClaimed = false;
    farm.questSpeedyClaimed = false;
    farm.questNightOwlClaimed = false;
    farm.questStreakSaverClaimed = false;
  }
}

// Applies `pages` worth of meals (PetConfig::PAGES_PER_MEAL pages = 1 meal) to
// one plot's hunger/weight/waste, shared by both the lazy reading-stats sync
// and the real-time page-turn hook.
void PetManager::feedFromPages(PetState& plot, uint32_t pages) {
  plot.pageAccumulator = static_cast<uint16_t>(
      std::min<uint32_t>(plot.pageAccumulator + pages, 60000));

  uint32_t meals = plot.pageAccumulator / PetConfig::PAGES_PER_MEAL;
  if (meals == 0) return;
  if (meals > 50) meals = 50;  // cap to prevent stat overflow
  plot.pageAccumulator = static_cast<uint16_t>(plot.pageAccumulator % PetConfig::PAGES_PER_MEAL);

  for (uint32_t i = 0; i < meals; i++) {
    plot.hunger = clampAdd(plot.hunger, PetConfig::HUNGER_PER_MEAL);
    plot.weight = clampAdd(plot.weight, PetConfig::WEIGHT_PER_MEAL);
    plot.mealsSinceClean++;
    if (plot.mealsSinceClean >= PetConfig::MEALS_UNTIL_WASTE) {
      plot.mealsSinceClean = 0;
      if (plot.wasteCount < PetConfig::MAX_WASTE) plot.wasteCount++;
    }
  }

  if (plot.health < PetConfig::MAX_STAT && plot.hunger > 0)
    plot.health = clampAdd(plot.health, 5);
  uint8_t happinessBonus = static_cast<uint8_t>(meals * 3 > 30 ? 30 : meals * 3);
  plot.happiness = clampAdd(plot.happiness, happinessBonus);
}

// --- Sync from GlobalReadingStats (lazy evaluation) ---

void PetManager::syncFromReadingStats(const GlobalReadingStats& stats) {
  if (!anyPlotExists(plots, farm)) return;

  // First-sync migration guard: don't retroactively apply all historical reading
  if (farm.lastKnownReadSeconds == 0 && farm.lastKnownPagesTurned == 0 && plots[0].lastTickTime > 0) {
    farm.lastKnownReadSeconds = stats.totalReadingSeconds;
    farm.lastKnownPagesTurned = stats.totalPagesTurned;
    farm.lastUpdateTimestamp = getCurrentTime();
    LOG_DBG("PET", "Migration: initialized lastKnownReadSeconds=%lu lastKnownPagesTurned=%lu",
            (unsigned long)stats.totalReadingSeconds, (unsigned long)stats.totalPagesTurned);
    return;
  }

  // Sync streak and books from GlobalReadingStats
  ReadingStatsDateTime today;
  farm.currentStreak = getCurrentLocalReadingStatsDateTime(today) ? stats.currentReadingStreak(&today.date) : 0;
  farm.lastReadDay = getDayOfYear();  // keep in sync to prevent tick() from resetting streak
  if (stats.completedBooks > farm.booksFinished) {
    uint32_t diff = stats.completedBooks - farm.booksFinished;
    farm.inkPoints += diff * 100;
  }
  farm.booksFinished = (stats.completedBooks > 255) ? 255 : static_cast<uint8_t>(stats.completedBooks);

  farm.longestReadingStreak = stats.longestReadingStreak;

  if (farm.lastKnownSessions == 0) {
    farm.lastKnownSessions = stats.totalSessions;
  } else if (stats.totalSessions > farm.lastKnownSessions) {
    uint32_t diffSessions = stats.totalSessions - farm.lastKnownSessions;
    farm.inkPoints += diffSessions * 5;
    farm.lastKnownSessions = stats.totalSessions;
  }

  if (farm.lastKnownReadSeconds == 0) {
    farm.lastKnownReadSeconds = stats.totalReadingSeconds;
  } else if (stats.totalReadingSeconds > farm.lastKnownReadSeconds) {
    uint32_t diffSeconds = stats.totalReadingSeconds - farm.lastKnownReadSeconds;
    farm.inkPoints += diffSeconds / 30;
    farm.lastKnownReadSeconds = stats.totalReadingSeconds;
  }

  // Feed + grow every owned/alive plot from any pages turned since the last
  // sync that onPageTurned() hasn't already accounted for (e.g. pet screen
  // opened without reading in between). Each plot accumulates its own
  // totalPagesRead independently — unlike the old single-pet code, which just
  // mirrored the account's lifetime page count — so plots started at
  // different times evolve independently instead of in lockstep.
  if (stats.totalPagesTurned > farm.lastKnownPagesTurned) {
    uint32_t pagesDelta = stats.totalPagesTurned - farm.lastKnownPagesTurned;
    for (int i = 0; i < farm.ownedPlotCount; i++) {
      PetState& plot = plots[i];
      if (!plot.exists() || !plot.isAlive()) continue;
      plot.totalPagesRead += pagesDelta;
      feedFromPages(plot, pagesDelta);
    }
  }

  // Update streak tier
  if (farm.currentStreak >= 30)      farm.streakTier = 3;
  else if (farm.currentStreak >= 14) farm.streakTier = 2;
  else if (farm.currentStreak >= 7)  farm.streakTier = 1;
  else                                farm.streakTier = 0;

  farm.lastKnownReadSeconds = stats.totalReadingSeconds;
  farm.lastKnownPagesTurned = stats.totalPagesTurned;
  farm.lastUpdateTimestamp = getCurrentTime();
  LOG_DBG("PET", "Synced: streak=%d books=%d", farm.currentStreak, farm.booksFinished);
}

void PetManager::onPageTurned() {
  load();
  updateSleepState();
  // Dew currency and daily-quest progress are account-level rewards for the act
  // of *reading*, so they accrue whenever the account has a plant at all — they
  // must NOT be gated on whether plots are awake or healthy. Plots sleep nightly
  // (10PM-7AM) and can fall sick, and previously this early-returned unless a
  // plot was alive AND awake AND not sick, so anyone reading at night (exactly
  // what the Night Owl quest rewards) or with a sick plant silently earned zero
  // Dew and made no quest progress. The per-plot growth loop below still skips
  // asleep/sick plots individually, so the plants themselves still rest — only
  // the shared currency/quests keep counting.
  if (!anyPlotExists(plots, farm)) return;

  resetMissionsIfNewDay();

  // Quest progress and currency are farm-level: one increment per read page
  // regardless of how many plots are owned.
  // 1. Daily Reading Goal (30 pages)
  if (farm.missionPagesRead < 30) {
    farm.missionPagesRead++;
    if (farm.missionPagesRead >= 30 && !farm.questReadClaimed) {
      farm.inkPoints += 50;
      farm.questReadClaimed = true;
    }
  }

  // 2. Speedy Reader (15 pages in one session)
  sessionPagesRead++;
  if (sessionPagesRead > farm.maxSessionPagesToday) {
    farm.maxSessionPagesToday = sessionPagesRead;
    if (farm.maxSessionPagesToday >= 15 && !farm.questSpeedyClaimed) {
      farm.inkPoints += 30;
      farm.questSpeedyClaimed = true;
    }
  }

  // 3. Night Owl (10 pages after 9 PM)
  struct tm timeinfo;
  if (localTimeNow(timeinfo)) {
    if (timeinfo.tm_hour >= 21) {
      if (farm.pagesReadAfter9PM < 10) {
        farm.pagesReadAfter9PM++;
        if (farm.pagesReadAfter9PM >= 10 && !farm.questNightOwlClaimed) {
          farm.inkPoints += 30;
          farm.questNightOwlClaimed = true;
        }
      }
    }
  }

  // 4. Streak Saver (3 day reading streak)
  if (farm.currentStreak >= 3 && !farm.questStreakSaverClaimed) {
    farm.inkPoints += 40;
    farm.questStreakSaverClaimed = true;
  }

  farm.inkPoints++;

  // Feed + grow every owned plot that's currently able to benefit (alive,
  // awake, healthy) — reading shouldn't dilute across plots or get wasted on
  // ones that happen to be asleep/sick right now.
  for (int i = 0; i < farm.ownedPlotCount; i++) {
    PetState& plot = plots[i];
    if (!plot.exists() || !plot.isAlive() || plot.isSleeping || plot.isSick) continue;
    plot.totalPagesRead++;
    feedFromPages(plot, 1);
  }
  farm.lastKnownPagesTurned++;
  // Not saved here — per-page-turn SD writes are a debounce violation.
  // EpubReaderActivity::onExit() persists this via PET_MANAGER.save() when
  // the reading session ends.
}

void PetManager::onChapterFinished() {
  load();
  // Account-level reward: the chapter bonus accrues whenever a plant exists,
  // regardless of the nightly sleep window or sickness (see onPageTurned).
  if (!anyPlotExists(plots, farm)) return;

  farm.inkPoints += 10;
}

// --- User interaction ---

bool PetManager::pet() {
  PetState& plot = activePlot();
  if (!plot.exists() || !plot.isAlive()) return false;

  unsigned long now = millis();
  if (now - lastPetTimeMs < PetConfig::PET_COOLDOWN_MS) return false;

  lastPetTimeMs = now;
  plot.happiness = clampAdd(plot.happiness, PetConfig::HAPPINESS_PER_PET);
  resetMissionsIfNewDay();
  if (farm.missionPetCount < 5) {
    farm.missionPetCount++;
    if (farm.missionPetCount >= 5 && !farm.questPetClaimed) {
      farm.inkPoints += 30;
      farm.questPetClaimed = true;
    }
  }
  LOG_DBG("PET", "Tended! happiness=%d", plot.happiness);
  save();
  return true;
}

void PetManager::resetData() {
  farm = PetFarmState();
  for (auto& plot : plots) plot = PetState();
  Storage.remove(PetConfig::STATE_PATH);
  loaded = true;
  save();
}

void PetManager::applyPremiumFertilizer() {
  PetState& plot = activePlot();
  plot.hunger = 100;
  plot.happiness = 100;
  plot.health = 100;
  plot.discipline = 100;
  plot.isSick = false;
  save();
}

void PetManager::updateWeather(uint8_t condition, int temp) {
  farm.weatherCondition = condition;
  farm.weatherTemp = temp;
  farm.lastWeatherSync = getCurrentTime();
  save();
}

void PetManager::forceWeatherSync() {
  farm.lastWeatherSync = 0;
  save();
}

void PetManager::refillWater() {
  farm.waterStock = 3;
  save();
}

void PetManager::refillFertilizer() {
  farm.fertilizerStock = 3;
  save();
}

void PetManager::hatchNew(const char* name, uint8_t type) {
  PetState& plot = activePlot();
  plot = PetState();  // Resets all fields to struct defaults
  plot.initialized = true;
  plot.stage = PetStage::EGG;
  plot.hunger = 80;
  plot.happiness = 80;
  plot.health = 100;
  plot.petType = type;
  if (name && name[0]) {
    strncpy(plot.petName, name, sizeof(plot.petName) - 1);
    plot.petName[sizeof(plot.petName) - 1] = '\0';
  }
  if (isTimeValid()) {
    plot.birthTime = getCurrentTime();
    plot.lastTickTime = plot.birthTime;
  }
  save();
  LOG_DBG("PET", "New egg hatched in plot %d! name='%s' type=%d", farm.activePlotIndex, plot.petName, plot.petType);
}

bool PetManager::renamePet(const char* name) {
  PetState& plot = activePlot();
  if (!plot.exists()) return false;
  if (name && name[0]) {
    strncpy(plot.petName, name, sizeof(plot.petName) - 1);
    plot.petName[sizeof(plot.petName) - 1] = '\0';
  } else {
    plot.petName[0] = '\0';
  }
  return save();
}

bool PetManager::changeType(uint8_t type) {
  PetState& plot = activePlot();
  if (!plot.exists()) return false;
  plot.petType = type;
  return save();
}

// --- Growing plots ---

void PetManager::switchPlot(int direction) {
  if (farm.ownedPlotCount <= 1) return;
  int idx = static_cast<int>(farm.activePlotIndex);
  idx = (idx + direction + farm.ownedPlotCount) % farm.ownedPlotCount;
  farm.activePlotIndex = static_cast<uint8_t>(idx);
  // Not saved: switching plots is a hot-path navigation action (like moving
  // the action-menu cursor), not a persistent progress change. Worst case on
  // power loss, the app reopens on the last-saved active plot.
}

bool PetManager::unlockNextPlot(uint32_t price) {
  if (farm.ownedPlotCount >= PetConfig::MAX_PLOTS) return false;
  if (farm.inkPoints < price) return false;
  farm.inkPoints -= price;
  farm.ownedPlotCount++;
  save();
  return true;
}

// --- State queries ---

PetMood PetManager::getMood() const {
  const PetState& plot = getState();
  if (!plot.isAlive())  return PetMood::DEAD;
  if (plot.isSleeping)  return PetMood::SLEEPING;
  if (plot.isSick)      return PetMood::SICK;
  if (plot.attentionCall) return PetMood::NEEDY;
  // Low discipline → occasional refusal behaviour
  if (plot.discipline < 30 && random(100) < 20) return PetMood::REFUSING;
  if (plot.hunger > 70 && plot.health > 70) return PetMood::HAPPY;
  if (plot.hunger > 30 && plot.health > 30) return PetMood::NEUTRAL;
  return PetMood::SAD;
}

uint32_t PetManager::getDaysAlive() const {
  const PetState& plot = getState();
  if (!plot.exists() || plot.birthTime == 0 || !isTimeValid()) return 0;
  uint32_t now = getCurrentTime();
  if (now <= plot.birthTime) return 0;
  return (now - plot.birthTime) / 86400;
}

void PetManager::startReadingSession() {
  sessionPagesRead = 0;
}

void PetManager::getMissions(PetMission out[6]) const {
  out[0] = {tr(STR_PET_MISSION_READ), farm.missionPagesRead, 30, farm.missionPagesRead >= 30, 50};
  out[1] = {tr(STR_PET_MISSION_PET),  farm.missionPetCount,   5, farm.missionPetCount  >=  5, 30};
  out[2] = {tr(STR_PET_MISSION_WATER), farm.missionWaterCount,  3, farm.missionWaterCount >=  3, 20};
  out[3] = {tr(STR_PET_MISSION_SPEEDY), farm.maxSessionPagesToday, 15, farm.maxSessionPagesToday >= 15, 30};
  out[4] = {tr(STR_PET_MISSION_NIGHTOWL), farm.pagesReadAfter9PM, 10, farm.pagesReadAfter9PM >= 10, 30};
  out[5] = {tr(STR_PET_MISSION_STREAK), (uint8_t)farm.currentStreak, 3, farm.currentStreak >= 3, 40};
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
  struct tm timeinfo;
  const bool haveTime = localTimeNow(timeinfo);
  const bool sleeping = haveTime && (timeinfo.tm_hour >= PetConfig::SLEEP_HOUR || timeinfo.tm_hour < PetConfig::WAKE_HOUR);
  for (int i = 0; i < farm.ownedPlotCount; i++) {
    if (!plots[i].exists()) continue;
    plots[i].isSleeping = sleeping;
  }
}

uint8_t PetManager::clampSub(uint8_t val, uint8_t amount) {
  return (val > amount) ? (val - amount) : 0;
}

uint8_t PetManager::clampAdd(uint8_t val, uint8_t amount) {
  uint16_t result = static_cast<uint16_t>(val) + amount;
  return (result > PetConfig::MAX_STAT) ? PetConfig::MAX_STAT : static_cast<uint8_t>(result);
}
