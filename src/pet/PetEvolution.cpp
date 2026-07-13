#include "PetEvolution.h"

#include <I18n.h>
#include <Logging.h>

namespace {

// Determine reading-based variant at an evolution branching point.
// Returns 0=Scholar, 1=Balanced, 2=Wild.
// Called after state.stage is already advanced; prevStageIdx = new stage index - 1.
static uint8_t determineVariant(const PetState& state, uint16_t currentStreak, uint8_t booksFinished) {
  uint8_t prevStageIdx = static_cast<uint8_t>(state.stage) - 1;
  uint16_t minPages = PetConfig::EVOLUTION[prevStageIdx].minPages;
  uint16_t scholarThreshold = minPages + (minPages / 2);  // 1.5x

  // Scholar: active reader with streaks and at least one book finished
  if (currentStreak >= 7 &&
      booksFinished >= 1 &&
      state.totalPagesRead >= scholarThreshold) {
    return 0;  // Scholar
  }

  // Wild: barely reading, no streak maintenance
  if (currentStreak < 3 &&
      state.totalPagesRead <= minPages + 50) {
    return 2;  // Wild
  }

  return 1;  // Balanced (default)
}

// Base (non-variant) stage name in species-specific vocabulary. Companion/Elder are the same
// "Mature"/"Prized" wording across all species (see PetEvolution.h).
const char* baseStageName(PetStage stage, uint8_t petType) {
  switch (stage) {
    case PetStage::EGG:
      switch (petType) {
        case 1:  return tr(STR_PET_STAGE_EGG_BEGONIA);
        case 2:  return tr(STR_PET_STAGE_EGG_ALOCASIA);
        default: return tr(STR_PET_STAGE_EGG_MONSTERA);
      }
    case PetStage::HATCHLING:
      switch (petType) {
        case 1:  return tr(STR_PET_STAGE_HATCHLING_BEGONIA);
        case 2:  return tr(STR_PET_STAGE_HATCHLING_ALOCASIA);
        default: return tr(STR_PET_STAGE_HATCHLING_MONSTERA);
      }
    case PetStage::YOUNGSTER:
      switch (petType) {
        case 1:  return tr(STR_PET_STAGE_YOUNGSTER_BEGONIA);
        case 2:  return tr(STR_PET_STAGE_YOUNGSTER_ALOCASIA);
        default: return tr(STR_PET_STAGE_YOUNGSTER_MONSTERA);
      }
    case PetStage::COMPANION: return tr(STR_PET_STAGE_COMPANION);
    case PetStage::ELDER:     return tr(STR_PET_STAGE_ELDER);
    default:                  return "???";
  }
}

}  // namespace

namespace PetEvolution {

void checkEvolution(PetState& state, uint16_t currentStreak, uint8_t booksFinished) {
  uint8_t stageIdx = static_cast<uint8_t>(state.stage);
  if (stageIdx >= static_cast<uint8_t>(PetStage::ELDER)) return;

  const auto& req = PetConfig::EVOLUTION[stageIdx];
  if (state.daysAtStage < req.minDays ||
      state.totalPagesRead < req.minPages ||
      state.hunger < req.minAvgHunger) return;

  // Additional reading gate for Companion → Elder
  if (stageIdx == 3) {
    if (currentStreak < 7 || booksFinished < 1) return;
  }

  // Advance to next stage
  state.stage = static_cast<PetStage>(stageIdx + 1);
  state.daysAtStage = 0;

  // Assign variant at branching stages (YOUNGSTER and COMPANION)
  if (state.stage == PetStage::YOUNGSTER || state.stage == PetStage::COMPANION) {
    state.evolutionVariant = determineVariant(state, currentStreak, booksFinished);
  }

  LOG_DBG("PET", "Evolved to stage %d variant %d", (int)state.stage, state.evolutionVariant);
}

const char* stageName(PetStage stage, uint8_t petType) {
  return baseStageName(stage, petType);
}

const char* variantStageName(PetStage stage, uint8_t variant, uint8_t petType) {
  switch (stage) {
    case PetStage::YOUNGSTER:
      if (variant == 0) return tr(STR_PET_STAGE_SCHOLARLY_YOUNG);
      if (variant == 2) return tr(STR_PET_STAGE_WILD_YOUNGSTER);
      return baseStageName(stage, petType);
    case PetStage::COMPANION:
      if (variant == 0) return tr(STR_PET_STAGE_SCHOLAR);
      if (variant == 2) return tr(STR_PET_STAGE_WILD_COMPANION);
      return baseStageName(stage, petType);
    default:
      return baseStageName(stage, petType);
  }
}

const char* typeName(uint8_t type) {
  switch (type) {
    case 0: return tr(STR_PET_TYPE_DEFAULT);
    case 1: return tr(STR_PET_TYPE_CAT);
    case 2: return tr(STR_PET_TYPE_DOG);
    default: return tr(STR_PET_TYPE_DEFAULT);
  }
}

}  // namespace PetEvolution
