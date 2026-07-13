#pragma once

#include "PetState.h"

// Handles pet evolution transitions and variant (quality) determination.
// Variant is assigned at HATCHLING→YOUNGSTER and YOUNGSTER→COMPANION transitions:
//   0 = good (high care score, normal weight, discipline)
//   1 = chubby (overweight)
//   2 = misbehaved (low discipline)
namespace PetEvolution {

  // Check and apply an evolution transition if requirements are met.
  // Assigns evolutionVariant at branching stages. currentStreak/booksFinished
  // are farm-level (PetFarmState) — reading habits are account-wide, not
  // tracked per plot — so the caller passes them in explicitly.
  void checkEvolution(PetState& state, uint16_t currentStreak, uint8_t booksFinished);

  // Returns the base (non-variant) display name for a stage, in species-specific vocabulary
  // (e.g. Monstera's Egg is "Seed", Alocasia's is "Corm"). Companion/Elder use the same "Mature"/
  // "Prized" wording across all species.
  const char* stageName(PetStage stage, uint8_t petType);

  // Returns display name for stage + variant + species combination. Falls back to stageName()
  // except at the YOUNGSTER/COMPANION branching stages, where variant 0/2 override with a
  // species-agnostic flourish name (Scholar/Wild) instead of the species word.
  const char* variantStageName(PetStage stage, uint8_t variant, uint8_t petType);

  // Returns translated display name for pet type index.
  const char* typeName(uint8_t type);

}  // namespace PetEvolution
