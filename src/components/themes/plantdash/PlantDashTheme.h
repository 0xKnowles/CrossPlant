#pragma once

#include <cstdint>

#include "components/themes/dashboard/DashboardTheme.h"

namespace PlantDashMetrics {
// Same tile envelope as Dashboard: the home activity hands the theme one
// full-width tile (y = homeTopPadding, height = homeCoverTileHeight) and
// draws only header + button hints around it.
constexpr ThemeMetrics values = DashboardMetrics::values;
}  // namespace PlantDashMetrics

// "Plant Dash" — a plant-forward home dashboard. Two rounded cards stacked
// vertically: a compact "Now Reading" card (cover, title, progress bar and a
// 2x2 reading-stat grid) on top, and a "My Plants" garden card (large plant
// sprite, name/stage, four vital bars and today's quest) below, finished with
// a one-line lifetime stats footer. Inherits all non-home behaviour from
// DashboardTheme.
class PlantDashTheme : public DashboardTheme {
 public:
  // Cover art size for the "Now Reading" card. Exposed so HomeActivity can
  // generate/locate a thumbnail at the exact size this theme draws, instead
  // of reusing Dashboard's larger portrait thumb.
  static constexpr int kCoverImageWidth = 200;
  static constexpr int kCoverImageHeight = 308;

  void drawRecentBookCover(GfxRenderer& renderer, Rect rect, const std::vector<RecentBook>& recentBooks,
                           int selectorIndex, bool& coverRendered, bool& coverBufferStored, bool& bufferRestored,
                           const std::function<bool()>& storeCoverBuffer, const BookReadingStats* stats = nullptr,
                           float progressPercent = -1.0f, const GlobalReadingStats* globalStats = nullptr,
                           const char* currentChapterTitle = nullptr) const override;
};
