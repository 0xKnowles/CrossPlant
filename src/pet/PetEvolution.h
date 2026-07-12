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

  // Returns display name for stage + variant combination.
  const char* variantStageName(PetStage stage, uint8_t variant);

  // Returns translated display name for pet type index.
  const char* typeName(uint8_t type);

}  // namespace PetEvolution
