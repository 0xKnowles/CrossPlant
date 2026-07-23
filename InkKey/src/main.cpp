// InkKey — air-gapped TOTP authenticator firmware for the Xteink X3.
//
// Security model in one paragraph: secrets enter once over USB serial while
// the device is unlocked and in provisioning mode, live AES-256-GCM-encrypted
// in NVS under a key derived from a 6-digit PIN, and codes are computed from
// the DS3231's battery-backed UTC clock. This binary links no WiFi and no
// Bluetooth code — after provisioning the device never needs any connection
// again. Deep sleep powers RAM down, so every wake starts locked.

#include <Arduino.h>
#include <BatteryMonitor.h>
#include <BoardConfig.h>
#include <EInkDisplay.h>
#include <FreeInkApp.h>
#include <FreeInkUIDisplayTarget.h>
#include <InputManager.h>
#include <esp_sleep.h>

#include <cstdio>
#include <cstring>

#include "core/Clock.h"
#include "core/Console.h"
#include "core/Totp.h"
#include "core/Vault.h"
#include "ui/fonts/CodeDigitsFont.h"
#include "ui/fonts/Medium24Font.h"

using freeink::ui::Color;
using freeink::ui::Rect;
using freeink::ui::RefreshHint;
using freeink::ui::TextAlign;
using freeink::ui::TextStyle;

using App = freeink::ui::FreeInkApp<32, 8>;

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------

static EInkDisplay display;
static InputManager buttons;
static BatteryMonitor* battery = nullptr;
static inkkey::Clock rtcClock;
static inkkey::Vault vault;
static inkkey::Console console;

static freeink::ui::DisplayTarget* target = nullptr;
static App* app = nullptr;

// Font slots on the DisplayTarget (slot 0 keeps the bundled UI font).
static constexpr freeink::ui::FontId FONT_UI = 0;
static constexpr freeink::ui::FontId FONT_MED = 1;
static constexpr freeink::ui::FontId FONT_CODE = 2;

enum class Mode : uint8_t {
  NoRtc,       // not an X3 / RTC dead — refuse to operate
  SetupPin,    // first run: choose a PIN (enter + confirm)
  Locked,      // PIN entry
  Home,        // account list + codes
  Menu,        // settings list
  Provision,   // USB console active
  WipeConfirm  // triple-confirm wipe
};

struct State {
  Mode mode = Mode::Locked;

  // PIN entry (setup + unlock share this).
  char pinBuf[inkkey::Vault::PIN_LEN + 1] = {0};
  uint8_t pinLen = 0;
  uint8_t pinDigit = 0;              // digit currently being adjusted
  char pinFirst[inkkey::Vault::PIN_LEN + 1] = {0};
  bool pinConfirmPhase = false;      // SetupPin: second entry
  bool pinError = false;             // last attempt failed (message flag)
  unsigned long backoffUntilMs = 0;  // wrong-PIN lockout deadline

  // Home.
  int16_t selected = 0;
  uint16_t topIndex = 0;
  uint64_t lastCodeStep = 0;
  uint8_t fastRefreshCount = 0;

  // Menu.
  int16_t menuSelected = 0;

  // Wipe confirm.
  uint8_t wipePresses = 0;

  uint16_t batteryPct = 0;
  unsigned long lastActivityMs = 0;
  unsigned long lastBatteryReadMs = 0;
};

static State state;

static constexpr unsigned long AUTOLOCK_SLEEP_MS = 3UL * 60UL * 1000UL;  // idle -> deep sleep
static constexpr uint8_t FULL_REFRESH_EVERY = 15;                        // code ticks between full refreshes

// ---------------------------------------------------------------------------
// Screens
// ---------------------------------------------------------------------------

static void drawHintLine(App::ScreenType& screen, const char* hint) {
  Rect band = screen.takeBottom(int16_t(screen.target().lineHeight(FONT_UI) + 4));
  TextStyle s;
  s.font = FONT_UI;
  s.align = TextAlign::Center;
  s.color = Color::DarkGray;
  screen.target().text(band, hint, s);
}

// header() overlaps title+subtitle with the large bundled font; draw the
// subtitle as its own centered line under the header band instead.
static void headerWithSub(App::ScreenType& screen, const char* title, const char* subtitle,
                          const char* rightLabel = nullptr) {
  screen.header(title, nullptr, rightLabel);
  if (subtitle && subtitle[0]) {
    screen.spacer(6);
    TextStyle s;
    s.font = FONT_UI;
    s.align = TextAlign::Center;
    Rect band = screen.takeTop(int16_t(screen.target().lineHeight(FONT_UI)), 2);
    screen.target().text(band, subtitle, s);
  }
}

static void drawPinBoxes(App::ScreenType& screen) {
  constexpr int16_t BOX = 56;
  constexpr int16_t GAP = 12;
  const int16_t totalW = int16_t(inkkey::Vault::PIN_LEN * BOX + (inkkey::Vault::PIN_LEN - 1) * GAP);
  Rect band = screen.takeTop(BOX + 16, 8);
  int16_t x = int16_t(band.x + (band.width - totalW) / 2);
  const int16_t y = int16_t(band.y + 8);

  for (uint8_t i = 0; i < inkkey::Vault::PIN_LEN; i++) {
    Rect box{x, y, BOX, BOX};
    const bool active = (i == state.pinDigit);
    screen.target().stroke(box, freeink::ui::Paint::solid(Color::Black), active ? 3 : 1, 4);

    char glyph[2] = {0, 0};
    if (i < state.pinLen) {
      // Only the digit under the cursor is shown; committed digits are masked.
      glyph[0] = active ? state.pinBuf[i] : '*';
    } else if (active) {
      glyph[0] = state.pinBuf[i] ? state.pinBuf[i] : '0';
    }
    if (glyph[0]) {
      TextStyle s;
      s.font = FONT_MED;
      s.align = TextAlign::Center;
      Rect textRect{box.x, int16_t(box.y + (BOX - screen.target().lineHeight(FONT_MED)) / 2), box.width,
                    int16_t(screen.target().lineHeight(FONT_MED))};
      screen.target().text(textRect, glyph, s);
    }
    x = int16_t(x + BOX + GAP);
  }
}

static void pinScreen(App::ScreenType& screen, void*) {
  const bool setup = state.mode == Mode::SetupPin;
  headerWithSub(screen, "InkKey",
                setup ? (state.pinConfirmPhase ? "Confirm your PIN" : "Choose a 6-digit PIN") : "Locked");

  screen.spacer(50);
  drawPinBoxes(screen);
  screen.spacer(16);

  const uint32_t backoff = vault.backoffSeconds();
  if (state.mode == Mode::Locked && millis() < state.backoffUntilMs) {
    char msg[64];
    snprintf(msg, sizeof(msg), "Too many attempts - wait %lus",
             (unsigned long)((state.backoffUntilMs - millis()) / 1000 + 1));
    screen.centeredText(msg);
  } else if (state.pinError) {
    screen.centeredText(setup ? "PINs did not match - start over" : "Wrong PIN");
  } else if (state.mode == Mode::Locked && backoff > 0) {
    screen.centeredText("Careful - repeated failures add delays");
  }

  drawHintLine(screen, "UP/DOWN digit  OK next  BACK erase");
}

static void codeHero(App::ScreenType& screen) {
  const inkkey::Account& a = vault.account(uint8_t(state.selected));
  uint64_t now = rtcClock.now();

  TextStyle name;
  name.font = FONT_MED;
  name.align = TextAlign::Center;
  Rect nameRect = screen.takeTop(int16_t(screen.target().lineHeight(FONT_MED)), 4);
  char title[72];
  if (a.issuer[0] && a.label[0]) {
    snprintf(title, sizeof(title), "%s - %s", a.issuer, a.label);
  } else {
    snprintf(title, sizeof(title), "%s", a.issuer[0] ? a.issuer : a.label);
  }
  screen.target().text(nameRect, title, name);

  char code[12];
  inkkey::formatCode(inkkey::totp(a, now), a.digits, code);
  TextStyle codeStyle;
  codeStyle.font = FONT_CODE;
  codeStyle.align = TextAlign::Center;
  Rect codeRect = screen.takeTop(int16_t(screen.target().lineHeight(FONT_CODE)), 4);
  screen.target().text(codeRect, code, codeStyle);

  // Countdown bar: remaining fraction of the period.
  const uint32_t remaining = inkkey::totpSecondsRemaining(a, now);
  Rect barBand = screen.takeTop(18, 10);
  Rect bar{int16_t(barBand.x + 40), barBand.y, int16_t(barBand.width - 80), 10};
  screen.target().stroke(bar, freeink::ui::Paint::solid(Color::Black), 1);
  const uint16_t period = a.period ? a.period : 30;
  int16_t fillW = int16_t((int32_t(bar.width - 4) * int32_t(remaining)) / period);
  if (fillW > 0) {
    Rect fill{int16_t(bar.x + 2), int16_t(bar.y + 2), fillW, int16_t(bar.height - 4)};
    screen.target().fill(fill, freeink::ui::Paint::solid(Color::Black));
  }
}

static void homeScreen(App::ScreenType& screen, void*) {
  char batteryLabel[16];
  snprintf(batteryLabel, sizeof(batteryLabel), "%u%%", state.batteryPct);
  screen.header("InkKey", nullptr, batteryLabel);

  if (!rtcClock.timeValid()) {
    screen.spacer(8);
    TextStyle warn;
    warn.font = FONT_UI;
    warn.align = TextAlign::Center;
    Rect band = screen.takeTop(int16_t(screen.target().lineHeight(FONT_UI)), 4);
    screen.target().text(band, "! CLOCK NOT SET - codes are wrong. Menu > Provisioning, then `time`", warn);
  }

  if (vault.accountCount() == 0) {
    screen.centeredText("No accounts yet - open Menu, enter Provisioning mode,");
    drawHintLine(screen, "BACK menu  OK sleep");
    // Second line of the empty-state message.
    TextStyle s;
    s.font = FONT_UI;
    s.align = TextAlign::Center;
    Rect band = screen.body();
    Rect line{band.x, int16_t(band.y + band.height / 2 + screen.target().lineHeight(FONT_UI)), band.width,
              int16_t(screen.target().lineHeight(FONT_UI))};
    screen.target().text(line, "then send otpauth:// URIs over USB serial", s);
    return;
  }

  screen.spacer(24);
  codeHero(screen);
  screen.spacer(12);

  // Account list with live codes as row values.
  static char rowCodes[inkkey::Vault::MAX_ACCOUNTS][12];
  static char rowLabels[inkkey::Vault::MAX_ACCOUNTS][68];
  static freeink::ui::ListItem items[inkkey::Vault::MAX_ACCOUNTS];
  uint64_t now = rtcClock.now();
  for (uint8_t i = 0; i < vault.accountCount(); i++) {
    const inkkey::Account& a = vault.account(i);
    if (a.issuer[0] && a.label[0]) {
      snprintf(rowLabels[i], sizeof(rowLabels[i]), "%s - %s", a.issuer, a.label);
    } else {
      snprintf(rowLabels[i], sizeof(rowLabels[i]), "%s", a.issuer[0] ? a.issuer : a.label);
    }
    inkkey::formatCode(inkkey::totp(a, now), a.digits, rowCodes[i]);
    items[i] = freeink::ui::ListItem{};
    items[i].label = rowLabels[i];
    items[i].value = rowCodes[i];
    items[i].actionValue = int16_t(i);
  }

  drawHintLine(screen, "UP/DOWN select  BACK menu  OK sleep");

  freeink::ui::ListProps props;
  props.items = items;
  props.count = vault.accountCount();
  props.selectedIndex = state.selected;
  props.topIndex = state.topIndex;
  screen.list(props);
}

static const char* const MENU_ITEMS[] = {
    "Provisioning mode (USB console)", "Change PIN", "Wipe device", "Sleep now",
};
static constexpr int16_t MENU_COUNT = 4;

static void menuScreen(App::ScreenType& screen, void*) {
  screen.header("Menu");
  drawHintLine(screen, "UP/DOWN select  OK open  BACK home");

  static freeink::ui::ListItem items[MENU_COUNT];
  for (int16_t i = 0; i < MENU_COUNT; i++) {
    items[i] = freeink::ui::ListItem{};
    items[i].label = MENU_ITEMS[i];
    items[i].actionValue = i;
  }
  freeink::ui::ListProps props;
  props.items = items;
  props.count = MENU_COUNT;
  props.selectedIndex = state.menuSelected;
  screen.list(props);
}

static void provisionScreen(App::ScreenType& screen, void*) {
  headerWithSub(screen, "Provisioning mode", "USB serial console active");
  screen.spacer(24);

  char counts[48];
  snprintf(counts, sizeof(counts), "%u account(s) stored", vault.accountCount());

  char timeBuf[32];
  rtcClock.formatUtc(timeBuf, sizeof(timeBuf));
  char timeLine[48];
  snprintf(timeLine, sizeof(timeLine), "RTC: %s", timeBuf);

  const char* lines[] = {
      "Connect USB, 115200 baud, type `help`.",
      "",
      timeLine,
      counts,
      "",
      "time / add otpauth: / list / del / done",
  };
  TextStyle s;
  s.font = FONT_UI;
  s.align = TextAlign::Center;
  for (const char* line : lines) {
    Rect band = screen.takeTop(int16_t(screen.target().lineHeight(FONT_UI)), 2);
    if (line[0]) screen.target().text(band, line, s);
  }

  drawHintLine(screen, "BACK exit provisioning");
}

static void wipeConfirmScreen(App::ScreenType& screen, void*) {
  headerWithSub(screen, "Wipe device", "This destroys all accounts");
  screen.spacer(40);
  char msg[64];
  snprintf(msg, sizeof(msg), "Press CONFIRM %u more time(s) to wipe", 3 - state.wipePresses);
  screen.centeredText(msg);
  drawHintLine(screen, "BACK cancel");
}

static void noRtcScreen(App::ScreenType& screen, void*) {
  headerWithSub(screen, "InkKey", "Unsupported device");
  screen.spacer(60);
  screen.centeredText("No DS3231 RTC found. InkKey requires the Xteink X3.");
  drawHintLine(screen, "The X4 has no battery-backed clock - see README");
}

// ---------------------------------------------------------------------------
// Mode transitions
// ---------------------------------------------------------------------------

static void resetPinEntry() {
  memset(state.pinBuf, 0, sizeof(state.pinBuf));
  state.pinLen = 0;
  state.pinDigit = 0;
}

static void enterMode(Mode m) {
  state.mode = m;
  switch (m) {
    case Mode::SetupPin:
    case Mode::Locked:
      resetPinEntry();
      app->setScreen(pinScreen, nullptr, RefreshHint::Full);
      break;
    case Mode::Home:
      state.lastCodeStep = 0;  // force code recompute + repaint
      app->setScreen(homeScreen, nullptr, RefreshHint::Full);
      break;
    case Mode::Menu:
      state.menuSelected = 0;
      app->setScreen(menuScreen, nullptr, RefreshHint::Fast);
      break;
    case Mode::Provision:
      console.begin(&vault, &rtcClock);
      app->setScreen(provisionScreen, nullptr, RefreshHint::Fast);
      break;
    case Mode::WipeConfirm:
      state.wipePresses = 0;
      app->setScreen(wipeConfirmScreen, nullptr, RefreshHint::Fast);
      break;
    case Mode::NoRtc:
      app->setScreen(noRtcScreen, nullptr, RefreshHint::Full);
      break;
  }
}

static void goToSleep() {
  // Leave a lock-screen image on the panel; e-ink keeps it at zero power.
  app->setScreen(pinScreen, nullptr, RefreshHint::Full);
  vault.lock();
  state.mode = Mode::Locked;
  resetPinEntry();
  app->render();
  freeink::ui::present(display, RefreshHint::Full);
  display.deepSleep();

  const int8_t powerPin = BoardConfig::ACTIVE.input.power;
  esp_deep_sleep_enable_gpio_wakeup(1ULL << powerPin, ESP_GPIO_WAKEUP_GPIO_LOW);
  esp_deep_sleep_start();
}

// ---------------------------------------------------------------------------
// PIN entry handling
// ---------------------------------------------------------------------------

static void handlePinSubmit() {
  state.pinBuf[inkkey::Vault::PIN_LEN] = '\0';

  if (state.mode == Mode::SetupPin) {
    if (!state.pinConfirmPhase) {
      memcpy(state.pinFirst, state.pinBuf, sizeof(state.pinFirst));
      state.pinConfirmPhase = true;
      state.pinError = false;
      resetPinEntry();
      return;
    }
    if (memcmp(state.pinFirst, state.pinBuf, inkkey::Vault::PIN_LEN) != 0) {
      state.pinConfirmPhase = false;
      state.pinError = true;
      memset(state.pinFirst, 0, sizeof(state.pinFirst));
      resetPinEntry();
      return;
    }
    if (vault.create(state.pinBuf)) {
      memset(state.pinFirst, 0, sizeof(state.pinFirst));
      state.pinError = false;
      enterMode(Mode::Home);
    } else {
      state.pinError = true;
      resetPinEntry();
    }
    return;
  }

  // Unlock path.
  inkkey::Vault::UnlockResult r = vault.unlock(state.pinBuf);
  if (r == inkkey::Vault::UnlockResult::Ok) {
    state.pinError = false;
    state.selected = 0;
    state.topIndex = 0;
    enterMode(Mode::Home);
    return;
  }
  state.pinError = true;
  resetPinEntry();
  if (r == inkkey::Vault::UnlockResult::WrongPin) {
    vault.recordFailedAttempt();
    uint32_t backoff = vault.backoffSeconds();
    if (backoff > 0) state.backoffUntilMs = millis() + backoff * 1000UL;
  }
}

static void handlePinInput() {
  if (state.mode == Mode::Locked && millis() < state.backoffUntilMs) {
    return;  // input frozen during backoff; screen ticks via the loop below
  }

  char& current = state.pinBuf[state.pinDigit];
  if (current == 0) current = '0';

  if (buttons.wasPressed(InputManager::BTN_UP)) {
    current = (current == '9') ? '0' : char(current + 1);
    if (state.pinDigit >= state.pinLen) state.pinLen = uint8_t(state.pinDigit + 1);
    app->invalidate();
  }
  if (buttons.wasPressed(InputManager::BTN_DOWN)) {
    current = (current == '0') ? '9' : char(current - 1);
    if (state.pinDigit >= state.pinLen) state.pinLen = uint8_t(state.pinDigit + 1);
    app->invalidate();
  }
  if (buttons.wasPressed(InputManager::BTN_CONFIRM)) {
    if (state.pinDigit >= state.pinLen) state.pinLen = uint8_t(state.pinDigit + 1);
    if (state.pinLen >= inkkey::Vault::PIN_LEN && state.pinDigit == inkkey::Vault::PIN_LEN - 1) {
      handlePinSubmit();
    } else {
      state.pinDigit++;
    }
    app->invalidate();
  }
  if (buttons.wasPressed(InputManager::BTN_BACK)) {
    if (state.pinDigit > 0) {
      state.pinBuf[state.pinDigit] = 0;
      state.pinDigit--;
      state.pinLen = state.pinDigit;
    }
    app->invalidate();
  }
}

// ---------------------------------------------------------------------------
// Per-mode input handling
// ---------------------------------------------------------------------------

static void handleHomeInput() {
  const uint8_t n = vault.accountCount();
  if (buttons.wasPressed(InputManager::BTN_UP) || buttons.wasPressed(InputManager::BTN_LEFT)) {
    if (n > 0) {
      state.selected = int16_t((state.selected + n - 1) % n);
      app->invalidate();
    }
  }
  if (buttons.wasPressed(InputManager::BTN_DOWN) || buttons.wasPressed(InputManager::BTN_RIGHT)) {
    if (n > 0) {
      state.selected = int16_t((state.selected + 1) % n);
      app->invalidate();
    }
  }
  if (buttons.wasPressed(InputManager::BTN_BACK)) enterMode(Mode::Menu);
  if (buttons.wasPressed(InputManager::BTN_CONFIRM)) goToSleep();
}

static void handleMenuInput() {
  if (buttons.wasPressed(InputManager::BTN_UP)) {
    state.menuSelected = int16_t((state.menuSelected + MENU_COUNT - 1) % MENU_COUNT);
    app->invalidate();
  }
  if (buttons.wasPressed(InputManager::BTN_DOWN)) {
    state.menuSelected = int16_t((state.menuSelected + 1) % MENU_COUNT);
    app->invalidate();
  }
  if (buttons.wasPressed(InputManager::BTN_BACK)) enterMode(Mode::Home);
  if (buttons.wasPressed(InputManager::BTN_CONFIRM)) {
    switch (state.menuSelected) {
      case 0:
        enterMode(Mode::Provision);
        break;
      case 1:
        // Change PIN re-uses setup flow; vault stays unlocked underneath.
        state.pinConfirmPhase = false;
        state.pinError = false;
        enterMode(Mode::SetupPin);
        break;
      case 2:
        enterMode(Mode::WipeConfirm);
        break;
      case 3:
        goToSleep();
        break;
    }
  }
}

static void handleProvisionInput() {
  if (console.service()) app->invalidate();
  if (console.doneRequested() || buttons.wasPressed(InputManager::BTN_BACK)) {
    console.drain();
    enterMode(Mode::Home);
  }
}

static void handleWipeInput() {
  if (buttons.wasPressed(InputManager::BTN_BACK)) enterMode(Mode::Menu);
  if (buttons.wasPressed(InputManager::BTN_CONFIRM)) {
    state.wipePresses++;
    if (state.wipePresses >= 3) {
      vault.wipe();
      enterMode(Mode::SetupPin);
    } else {
      app->invalidate();
    }
  }
}

// ---------------------------------------------------------------------------
// setup / loop
// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);

  display.begin();
  buttons.begin();

  static BatteryMonitor batteryInstance;
  battery = &batteryInstance;

  static freeink::ui::DisplayTarget targetInstance(display.getFrameBuffer(), display.getDisplayWidth(),
                                                   display.getDisplayHeight(), display.getDisplayWidthBytes());
  static App appInstance(targetInstance, targetInstance.deviceContext());
  target = &targetInstance;
  app = &appInstance;

  target->setFont(FONT_MED, freeink::ui::kMedium24Font);
  target->setFont(FONT_CODE, freeink::ui::kCodeDigitsFont);
  app->setClearColor(Color::White);
  app->setTransitionFullEvery(6);

  const bool rtcOk = rtcClock.begin();
  state.batteryPct = battery->readPercentage();
  state.lastActivityMs = millis();

  if (!rtcOk) {
    enterMode(Mode::NoRtc);
    return;
  }

  // Apply persisted wrong-PIN backoff across reboots: rebooting doesn't skip
  // the wait, it restarts it.
  uint32_t backoff = vault.backoffSeconds();
  if (backoff > 0) state.backoffUntilMs = millis() + backoff * 1000UL;

  enterMode(vault.exists() ? Mode::Locked : Mode::SetupPin);
}

void loop() {
  buttons.update();

  if (buttons.wasAnyPressed()) state.lastActivityMs = millis();

  // Power button always sleeps immediately (except on the error screen, where
  // sleeping is also the only sensible action).
  if (buttons.wasPressed(InputManager::BTN_POWER)) {
    goToSleep();
  }

  switch (state.mode) {
    case Mode::NoRtc:
      break;
    case Mode::SetupPin:
    case Mode::Locked:
      handlePinInput();
      break;
    case Mode::Home:
      handleHomeInput();
      break;
    case Mode::Menu:
      handleMenuInput();
      break;
    case Mode::Provision:
      handleProvisionInput();
      break;
    case Mode::WipeConfirm:
      handleWipeInput();
      break;
  }

  // Serial input arriving outside provisioning mode is dropped unread.
  if (state.mode != Mode::Provision) console.drain();

  // Code rollover tick: repaint Home when the TOTP step changes.
  if (state.mode == Mode::Home && vault.accountCount() > 0 && rtcClock.timeValid()) {
    const inkkey::Account& a = vault.account(uint8_t(state.selected));
    const uint16_t period = a.period ? a.period : 30;
    const uint64_t step = rtcClock.now() / period;
    if (step != state.lastCodeStep) {
      state.lastCodeStep = step;
      state.fastRefreshCount++;
      app->invalidate(state.fastRefreshCount % FULL_REFRESH_EVERY == 0 ? RefreshHint::Full : RefreshHint::Fast);
    }
  }

  // Backoff countdown repaint (once per second).
  if (state.mode == Mode::Locked && state.backoffUntilMs > millis()) {
    static unsigned long lastBackoffPaint = 0;
    if (millis() - lastBackoffPaint > 1000) {
      lastBackoffPaint = millis();
      app->invalidate();
    }
  }

  // Battery refresh every 60 s.
  if (millis() - state.lastBatteryReadMs > 60000UL) {
    state.lastBatteryReadMs = millis();
    uint16_t pct;
    if (battery->readPercentageChecked(pct) && pct != state.batteryPct) {
      state.batteryPct = pct;
      if (state.mode == Mode::Home) app->invalidate();
    }
  }

  // Idle auto-sleep (which locks by construction).
  if (millis() - state.lastActivityMs > AUTOLOCK_SLEEP_MS && state.mode != Mode::Provision) {
    goToSleep();
  }

  if (app->invalidated()) {
    app->render();
    freeink::ui::present(display, app->lastRenderRefreshHint());
  }

  delay(15);
}
