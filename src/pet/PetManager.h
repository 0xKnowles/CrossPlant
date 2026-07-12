#pragma once

#include "PetState.h"

struct GlobalReadingStats;  // forward declaration (activities/reader/GlobalReadingStats.h)

// A single daily mission shown in the pet UI
struct PetMission {
  const char* label;
  uint8_t     progress;
  uint8_t     goal;
  bool        done;
  uint16_t    reward;
};

// Manages virtual pet game logic, stat decay, evolution, and persistence.
// Implementation split across PetManager.cpp, PetPersistence.cpp, PetActions.cpp.
// Singleton accessed via PET_MANAGER macro.
//
// Holds up to PetConfig::MAX_PLOTS independent growing plots plus one shared
// PetFarmState (currency, shop boosts, water/fertilizer stock, quests, reading
// streak, herbarium, weather). getState() always returns the *active* plot, so
// existing plot-level call sites (render code, PetActionMenu, PetStatsPanel)
// don't need to know about plots at all — only code that cares about currency,
// shop, quests, streak, or plot count needs getFarmState()/switchPlot().
class PetManager {
 public:
  PetManager(const PetManager&) = delete;
  PetManager& operator=(const PetManager&) = delete;

  static PetManager& getInstance();

  // Persistence (PetPersistence.cpp)
  bool load();
  bool save();

  // Game logic — call tick() to apply time-based decay (lazy: on pet app open)
  void tick();

  // Sync pet state from GlobalReadingStats (reading time → feeding, streak sync)
  void syncFromReadingStats(const GlobalReadingStats& stats);

  // Called on each recorded forward page turn during reading. Accumulates pages
  // toward the next meal and today's reading mission in real time, independent
  // of the lazy syncFromReadingStats() sync (which only runs when the pet
  // screen is opened).
  void onPageTurned();
  void onChapterFinished();

  // User interaction — petting gives happiness (with cooldown)
  bool pet();

  // Start a new pet from egg (optionally with custom name and type) in the
  // currently active plot.
  void hatchNew(const char* name = nullptr, uint8_t type = 0);

  // Rename/retype the active plot's pet
  bool renamePet(const char* name);
  bool changeType(uint8_t type);
  void resetData();
  void applyPremiumFertilizer();
  void updateWeather(uint8_t condition, int temp);
  void forceWeatherSync();
  void setFeedback(const char* msg) { lastFeedback = msg; }
  void refillWater();
  void refillFertilizer();

  // --- Growing plots ---
  int activePlotIndex() const { return farm.activePlotIndex; }
  int ownedPlotCount() const { return farm.ownedPlotCount; }
  static int maxPlots() { return PetConfig::MAX_PLOTS; }
  // Moves the active plot by +1/-1, wrapping within owned plots. No-op with 1 plot owned.
  void switchPlot(int direction);
  // Buys the next locked plot slot (deducts price, increments ownedPlotCount).
  // Returns false if already at MAX_PLOTS or insufficient Dew.
  bool unlockNextPlot(uint32_t price);

  // Shop & customization helper methods
  bool deductPoints(uint32_t points) {
    if (farm.inkPoints >= points) {
      farm.inkPoints -= points;
      return true;
    }
    return false;
  }
  void setHasPremiumSprayer(bool val) { farm.hasPremiumSprayer = val; save(); }
  void setHasMossPole(bool val) { farm.hasMossPole = val; save(); }
  void setEquipMossPole(bool val) { farm.equipMossPole = val; save(); }
  void setHasSelfWateringPot(bool val) { farm.hasSelfWateringPot = val; save(); }
  void setEquipSelfWateringPot(bool val) { farm.equipSelfWateringPot = val; save(); }
  void setHasSlowReleaseFertilizer(bool val) { farm.hasSlowReleaseFertilizer = val; save(); }
  void setEquipSlowReleaseFertilizer(bool val) { farm.equipSlowReleaseFertilizer = val; save(); }
  void setHasGreenhouseCover(bool val) { farm.hasGreenhouseCover = val; save(); }
  void setEquipGreenhouseCover(bool val) { farm.equipGreenhouseCover = val; save(); }

  // --- User actions (PetActions.cpp) ---
  bool feedMeal();       // fill hunger + add weight + waste tracking
  bool feedSnack();      // add happiness + add weight (no hunger)
  bool giveMedicine();   // cure sickness
  bool exercise();       // reduce weight + add happiness (1h cooldown)
  bool cleanBathroom();  // clear all waste piles
  bool disciplinePet();  // scold during attention call
  bool ignoreCry();      // ignore attention call (good if fake, bad if real)
  bool toggleLights();   // toggle sleep lights-off flag

  // State queries — all target the active plot
  const PetState& getState() const { return plots[farm.activePlotIndex]; }
  const PetFarmState& getFarmState() const { return farm; }
  PetMood getMood() const;
  bool isAlive() const { return getState().isAlive(); }
  bool exists() const { return getState().exists(); }
  uint32_t getDaysAlive() const;
  const char* getLastFeedback() const { return lastFeedback; }

  // Daily missions — returns 6 missions for today
  void getMissions(PetMission out[6]) const;
  void startReadingSession();

 private:
  PetManager() = default;

  PetFarmState farm;
  PetState plots[PetConfig::MAX_PLOTS];
  unsigned long lastPetTimeMs = 0;      // millis() of last petting (cooldown)
  unsigned long lastExerciseMs = 0;     // millis() of last exercise (cooldown)
  bool loaded = false;
  const char* lastFeedback = nullptr;   // feedback string for UI display
  uint8_t sessionPagesRead = 0;        // pages read in current reading session

  PetState& activePlot() { return plots[farm.activePlotIndex]; }

  // Internal helpers
  void updateStreak();
  bool isTimeValid() const;
  uint32_t getCurrentTime() const;
  uint16_t getDayOfYear() const;
  void resetMissionsIfNewDay();
  void feedFromPages(PetState& plot, uint32_t pages);
  void updateSleepState();

  static uint8_t clampSub(uint8_t val, uint8_t amount);
  static uint8_t clampAdd(uint8_t val, uint8_t amount);

  friend class PetPersistence;  // allow PetPersistence.cpp to access privates via method calls
};

#define PET_MANAGER PetManager::getInstance()
