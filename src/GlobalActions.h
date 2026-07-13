#pragma once

#include "CrossPointSettings.h"

inline bool isPowerButtonActionAvailableOutsideReader(const CrossPointSettings::SHORT_PWRBTN action) {
  switch (action) {
    case CrossPointSettings::SHORT_PWRBTN::SLEEP:
    case CrossPointSettings::SHORT_PWRBTN::FORCE_REFRESH:
    case CrossPointSettings::SHORT_PWRBTN::SYNC_PROGRESS:
    case CrossPointSettings::SHORT_PWRBTN::SCREENSHOT:
    case CrossPointSettings::SHORT_PWRBTN::FILE_TRANSFER:
    case CrossPointSettings::SHORT_PWRBTN::CALIBRE_WIRELESS:
    case CrossPointSettings::SHORT_PWRBTN::JOIN_NETWORK:
    case CrossPointSettings::SHORT_PWRBTN::CREATE_HOTSPOT:
    case CrossPointSettings::SHORT_PWRBTN::VIRTUAL_PET:
      return true;
    case CrossPointSettings::SHORT_PWRBTN::IGNORE:
    case CrossPointSettings::SHORT_PWRBTN::PAGE_TURN:
    case CrossPointSettings::SHORT_PWRBTN::TOGGLE_FONT:
    case CrossPointSettings::SHORT_PWRBTN::TOGGLE_GUIDE_DOTS:
    case CrossPointSettings::SHORT_PWRBTN::TOGGLE_BIONIC_READING:
    case CrossPointSettings::SHORT_PWRBTN::TOGGLE_BOOKMARK:
    case CrossPointSettings::SHORT_PWRBTN::MARK_FINISHED:
    case CrossPointSettings::SHORT_PWRBTN::READING_STATS:
    case CrossPointSettings::SHORT_PWRBTN::CYCLE_PAGE_TURN:
    case CrossPointSettings::SHORT_PWRBTN::TOGGLE_TILT_PAGE_TURN:
    case CrossPointSettings::SHORT_PWRBTN::TOGGLE_DARK_MODE:
    case CrossPointSettings::SHORT_PWRBTN::FOOTNOTES:
    case CrossPointSettings::SHORT_PWRBTN::FILE_BROWSER:
    case CrossPointSettings::SHORT_PWRBTN::CREATE_CLIPPING:
    case CrossPointSettings::SHORT_PWRBTN::SHORT_PWRBTN_COUNT:
    default:
      return false;
  }
}

void enterDeepSleep(bool fromTimeout = false);

// Mirrors isPowerButtonActionAvailableOutsideReader() above for the Back/Confirm long-press
// quick actions (SETTINGS.longPressBackAction / longPressMenuAction). These two settings are
// dispatched from two places: inside the reader (EpubReaderActivity::executeReaderQuickAction(),
// which supports the full book-aware action set) and, for everywhere else in the app, here —
// only actions that don't need an open book are available outside the reader.
inline bool isBackConfirmLongPressActionAvailableOutsideReader(const CrossPointSettings::LONG_PRESS_MENU_ACTION action) {
  switch (action) {
    case CrossPointSettings::LONG_MENU_SLEEP:
    case CrossPointSettings::LONG_MENU_SCREENSHOT:
    case CrossPointSettings::LONG_MENU_READING_STATS:
    case CrossPointSettings::LONG_MENU_FILE_TRANSFER:
    case CrossPointSettings::LONG_MENU_CALIBRE_WIRELESS:
    case CrossPointSettings::LONG_MENU_JOIN_NETWORK:
    case CrossPointSettings::LONG_MENU_CREATE_HOTSPOT:
    case CrossPointSettings::LONG_MENU_VIRTUAL_PET:
    case CrossPointSettings::LONG_MENU_FILE_BROWSER:
      return true;
    case CrossPointSettings::LONG_MENU_OFF:
    case CrossPointSettings::LONG_MENU_CHANGE_FONT:
    case CrossPointSettings::LONG_MENU_TOGGLE_GUIDE_DOTS:
    case CrossPointSettings::LONG_MENU_TOGGLE_BIONIC:
    case CrossPointSettings::LONG_MENU_TOGGLE_BOOKMARK:
    case CrossPointSettings::LONG_MENU_REFRESH_SCREEN:
    case CrossPointSettings::LONG_MENU_SYNC_PROGRESS:
    case CrossPointSettings::LONG_MENU_MARK_FINISHED:
    case CrossPointSettings::LONG_MENU_CYCLE_PAGE_TURN:
    case CrossPointSettings::LONG_MENU_TOGGLE_TILT_PAGE_TURN:
    case CrossPointSettings::LONG_MENU_TOGGLE_DARK_MODE:
    case CrossPointSettings::LONG_MENU_FOOTNOTES:
    case CrossPointSettings::LONG_MENU_CREATE_CLIPPING:
    case CrossPointSettings::LONG_PRESS_MENU_ACTION_COUNT:
    default:
      return false;
  }
}
