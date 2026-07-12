#include "PetActionMenu.h"

#include <I18n.h>

#include "fontIds.h"

// ---- Navigation ---------------------------------------------------------

void PetActionMenu::moveUp() {
  selectedIndex = (selectedIndex > 0) ? selectedIndex - 1 : ACTION_TOTAL - 1;
}

void PetActionMenu::moveDown() {
  selectedIndex = (selectedIndex < ACTION_TOTAL - 1) ? selectedIndex + 1 : 0;
}

PetAction PetActionMenu::getSelected() const {
  return static_cast<PetAction>(selectedIndex);
}

// ---- Action availability guards -----------------------------------------

bool PetActionMenu::isActionAvailable(PetAction action, const PetState& state) const {
  if (!state.isAlive()) return false;

  switch (action) {
    case PetAction::FEED_MEAL:
    case PetAction::FEED_SNACK:
      return !state.isSleeping && !state.isSick;
    case PetAction::MEDICINE:
      return !state.isSleeping && state.isSick;
    case PetAction::EXERCISE:
      return !state.isSleeping && !state.isSick;
    case PetAction::CLEAN:
      return state.wasteCount > 0;
    case PetAction::SCOLD:
    case PetAction::IGNORE_CRY:
      return state.attentionCall;
    case PetAction::TOGGLE_LIGHTS:
      return true;
    case PetAction::PET_PET:
      return !state.isSleeping;
    case PetAction::DAILY_QUESTS:
    case PetAction::RENAME:
    case PetAction::CHANGE_TYPE:
    case PetAction::SHOP:
    case PetAction::ALBUM:
    case PetAction::CONNECT_WEATHER:
    case PetAction::REFILL_WATER:
    case PetAction::BUY_FERTILIZER:
    case PetAction::RESET_DATA:
      return true;
    default:
      return false;
  }
}

// ---- Action labels ------------------------------------------------------

void PetActionMenu::actionLabel(PetAction action, const PetState& state, char* outBuf, size_t bufSize) {
  switch (action) {
    case PetAction::FEED_MEAL:
      snprintf(outBuf, bufSize, "%s (%u/3)", tr(STR_PET_ACTION_FEED_MEAL), state.waterStock);
      break;
    case PetAction::SCOLD:
      snprintf(outBuf, bufSize, "%s (%u/3)", tr(STR_PET_ACTION_SCOLD), state.fertilizerStock);
      break;
    case PetAction::FEED_SNACK:      snprintf(outBuf, bufSize, "%s", tr(STR_PET_ACTION_FEED_SNACK)); break;
    case PetAction::MEDICINE:        snprintf(outBuf, bufSize, "%s", tr(STR_PET_ACTION_MEDICINE)); break;
    case PetAction::EXERCISE:        snprintf(outBuf, bufSize, "%s", tr(STR_PET_ACTION_EXERCISE)); break;
    case PetAction::CLEAN:           snprintf(outBuf, bufSize, "%s", tr(STR_PET_ACTION_CLEAN)); break;
    case PetAction::IGNORE_CRY:      snprintf(outBuf, bufSize, "%s", tr(STR_PET_ACTION_IGNORE)); break;
    case PetAction::TOGGLE_LIGHTS:   snprintf(outBuf, bufSize, "%s", tr(STR_PET_ACTION_LIGHTS)); break;
    case PetAction::PET_PET:         snprintf(outBuf, bufSize, "%s", tr(STR_PET_ACTION_PET)); break;
    case PetAction::DAILY_QUESTS:    snprintf(outBuf, bufSize, "%s", tr(STR_PET_ACTION_QUESTS)); break;
    case PetAction::RENAME:          snprintf(outBuf, bufSize, "%s", tr(STR_PET_ACTION_RENAME)); break;
    case PetAction::CHANGE_TYPE:     snprintf(outBuf, bufSize, "%s", tr(STR_PET_ACTION_TYPE)); break;
    case PetAction::SHOP:            snprintf(outBuf, bufSize, "%s", tr(STR_PET_ACTION_SHOP)); break;
    case PetAction::ALBUM:           snprintf(outBuf, bufSize, "%s", tr(STR_PET_ACTION_ALBUM)); break;
    case PetAction::CONNECT_WEATHER: snprintf(outBuf, bufSize, "%s", tr(STR_PET_ACTION_WEATHER)); break;
    case PetAction::REFILL_WATER:    snprintf(outBuf, bufSize, "%s", tr(STR_PET_ACTION_REFILL_WATER)); break;
    case PetAction::BUY_FERTILIZER:  snprintf(outBuf, bufSize, "%s", tr(STR_PET_ACTION_BUY_FERTILIZER)); break;
    case PetAction::RESET_DATA:      snprintf(outBuf, bufSize, "%s", tr(STR_PET_ACTION_RESET)); break;
    default:                         snprintf(outBuf, bufSize, "???"); break;
  }
}

// ---- Rendering ----------------------------------------------------------

void PetActionMenu::render(GfxRenderer& renderer, const PetState& state,
                           int x, int y, int w, int h) const {
  const int lh = renderer.getLineHeight(SMALL_FONT_ID);
  const int rowH = lh + 6;
  const int visibleRows = h / rowH;

  // Scroll offset: keep selected item visible
  int scrollOffset = 0;
  if (selectedIndex >= visibleRows) scrollOffset = selectedIndex - visibleRows + 1;

  for (int i = 0; i < ACTION_TOTAL && (i - scrollOffset) < visibleRows; i++) {
    if (i < scrollOffset) continue;
    
    int rowY = y + (i - scrollOffset) * rowH;
    if (i >= static_cast<int>(PetAction::DAILY_QUESTS)) {
      rowY += 10; // 10px spacing gap for separator 1
    }
    if (i >= static_cast<int>(PetAction::REFILL_WATER)) {
      rowY += 10; // additional 10px spacing gap for separator 2
    }

    const PetAction action = static_cast<PetAction>(i);
    const bool available = isActionAvailable(action, state);
    const bool selected = (i == selectedIndex);
    
    char label[64];
    actionLabel(action, state, label, sizeof(label));

    if (i == static_cast<int>(PetAction::DAILY_QUESTS)) {
      renderer.drawLine(x + 4, rowY - 6, x + w - 4, rowY - 6, true);
    }
    if (i == static_cast<int>(PetAction::REFILL_WATER)) {
      renderer.drawLine(x + 4, rowY - 6, x + w - 4, rowY - 6, true);
    }

    if (selected) {
      // Highlight selected row with an inverted rect
      renderer.fillRect(x, rowY, w, rowH);
      renderer.drawText(SMALL_FONT_ID, x + 4, rowY + 3, label, /*invert=*/false);
    } else {
      // No brackets for unavailable items, draw directly as requested
      renderer.drawText(SMALL_FONT_ID, x + 4, rowY + 3, label);
    }
  }
}
