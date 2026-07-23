// InkKey host simulator: compiles the real firmware sources (main.cpp, vault,
// console, UI) against the stub hardware in stubs/, drives the whole
// first-run -> provision -> unlock flow, and writes every e-ink frame as a
// PBM image. Build and run:
//
//   ./build_sim.sh && ./sim out/
//
// Exits non-zero if any scripted expectation fails.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "Arduino.h"
#include "EInkDisplay.h"
#include "InputManager.h"
#include "Preferences.h"
#include "Rtc.h"
#include "esp_sleep.h"

// Firmware entry points (defined in src/main.cpp).
void setup();
void loop();

// ---------------------------------------------------------------------------
// Stub state
// ---------------------------------------------------------------------------

HostSerial Serial;
bool hostDeepSleepEntered = false;
std::map<std::string, std::vector<uint8_t>> Preferences::store;

static unsigned long g_millis = 0;
unsigned long millis() { return g_millis; }
void delay(unsigned long ms) { g_millis += ms; }

// Button queue: one press edge per update().
static std::deque<uint8_t> g_buttonQueue;
void hostPressButton(uint8_t b) { g_buttonQueue.push_back(b); }
void hostInputPop(InputManager& im) {
  im.pressedEdge_ = -1;
  if (!g_buttonQueue.empty()) {
    im.pressedEdge_ = g_buttonQueue.front();
    g_buttonQueue.pop_front();
  }
}
void InputManager::update() { hostInputPop(*this); }

// RTC simulation: epoch-based, converted to civil time on read.
namespace freeink {
bool Rtc::present = true;
bool Rtc::timeSet = false;
Rtc::DateTime Rtc::current{};
unsigned long Rtc::setAtMillis = 0;

static uint64_t civilToEpoch(const Rtc::DateTime& dt) {
  int32_t y = dt.year, m = dt.month, d = dt.day;
  y -= m <= 2;
  const int32_t era = (y >= 0 ? y : y - 399) / 400;
  const uint32_t yoe = uint32_t(y - era * 400);
  const uint32_t doy = uint32_t((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);
  const uint32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  const int64_t days = int64_t(era) * 146097 + int64_t(doe) - 719468;
  return uint64_t(days * 86400 + int64_t(dt.hour) * 3600 + int64_t(dt.minute) * 60 + int64_t(dt.second));
}

static void epochToCivil(uint64_t t, Rtc::DateTime& out) {
  int64_t days = int64_t(t / 86400);
  uint32_t rem = uint32_t(t % 86400);
  days += 719468;
  const int64_t era = (days >= 0 ? days : days - 146096) / 146097;
  const uint32_t doe = uint32_t(days - era * 146097);
  const uint32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const int64_t y = int64_t(yoe) + era * 400;
  const uint32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const uint32_t mp = (5 * doy + 2) / 153;
  const uint32_t d = doy - (153 * mp + 2) / 5 + 1;
  const uint32_t m = mp < 10 ? mp + 3 : mp - 9;
  out.year = uint16_t(y + (m <= 2));
  out.month = uint8_t(m);
  out.day = uint8_t(d);
  out.hour = uint8_t(rem / 3600);
  out.minute = uint8_t((rem % 3600) / 60);
  out.second = uint8_t(rem % 60);
  out.weekday = uint8_t(((t / 86400) + 4) % 7);
}

bool Rtc::now(DateTime& out) {
  if (!present) return false;
  if (!timeSet) {
    out = DateTime{};  // year 2000: reads as "oscillator ran, never programmed"
    return true;
  }
  uint64_t epoch = civilToEpoch(current) + (millis() - setAtMillis) / 1000;
  epochToCivil(epoch, out);
  return true;
}
}  // namespace freeink

// Frame capture.
static std::string g_outDir = "out";
static std::string g_stage = "boot";
static int g_frameCounter = 0;

void (*EInkDisplay::onPresent)(const uint8_t* fb) = nullptr;
void EInkDisplay::displayBuffer(RefreshMode, bool) {
  if (onPresent) onPresent(fb_);
}

static void writeFrame(const uint8_t* fb) {
  char path[256];
  snprintf(path, sizeof(path), "%s/%02d-%s.pbm", g_outDir.c_str(), g_frameCounter++, g_stage.c_str());
  FILE* f = fopen(path, "wb");
  if (!f) return;
  fprintf(f, "P4\n%d %d\n", EInkDisplay::W, EInkDisplay::H);
  // Framebuffer: bit set = white. PBM: bit set = black. Invert.
  for (size_t i = 0; i < size_t(EInkDisplay::WB) * EInkDisplay::H; i++) fputc(uint8_t(~fb[i]), f);
  fclose(f);
}

// ---------------------------------------------------------------------------
// Scenario driver
// ---------------------------------------------------------------------------

static int g_failures = 0;

static void expect(bool ok, const char* what) {
  printf("%s %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) g_failures++;
}

static void runLoops(int n) {
  for (int i = 0; i < n; i++) loop();
}

static void press(uint8_t button, int settleLoops = 3) {
  hostPressButton(button);
  runLoops(settleLoops);
}

static bool serialSaw(const char* needle) { return Serial.tx.find(needle) != std::string::npos; }

int main(int argc, char** argv) {
  if (argc > 1) g_outDir = argv[1];
  EInkDisplay::onPresent = writeFrame;

  // --- First boot: no vault, RTC present but time never set ---
  g_stage = "setup-pin";
  setup();
  runLoops(3);

  // Choose PIN 000000: six CONFIRM presses commit the default '0' per digit.
  for (int i = 0; i < 6; i++) press(InputManager::BTN_CONFIRM);
  g_stage = "setup-pin-confirm";
  runLoops(2);
  for (int i = 0; i < 6; i++) press(InputManager::BTN_CONFIRM);
  g_stage = "home-empty";
  runLoops(2);
  expect(Preferences::store.count("vault") == 1, "vault created after PIN setup");

  // --- Menu -> provisioning ---
  g_stage = "menu";
  press(InputManager::BTN_BACK);
  g_stage = "provisioning";
  press(InputManager::BTN_CONFIRM);

  // --- Provision over the console: set time, add three accounts ---
  Serial.inject("time 2026-07-23T12:00:00Z\n");
  runLoops(3);
  expect(serialSaw("OK RTC set to 2026-07-23 12:00:0"), "console sets RTC");

  Serial.inject(
      "add otpauth://totp/GitHub:alice%40example.com?secret=GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ&issuer=GitHub\n");
  Serial.inject("add otpauth://totp/Proton:knowles?secret=JBSWY3DPEHPK3PXP&issuer=Proton\n");
  Serial.inject("add otpauth://totp/Cloudflare:ops?secret=MFRGGZDFMZTWQ2LK&issuer=Cloudflare&digits=8\n");
  runLoops(3);
  expect(serialSaw("OK added GitHub / alice@example.com"), "console adds GitHub account");
  expect(serialSaw("OK added Cloudflare / ops"), "console adds 8-digit account");

  Serial.inject("list\n");
  runLoops(2);
  expect(serialSaw("[8 digits, 30s, SHA1]"), "list shows account parameters");

  Serial.inject("done\n");
  g_stage = "home-accounts";
  runLoops(3);
  expect(serialSaw("OK leaving provisioning mode"), "console done exits provisioning");

  // --- Switch selected account ---
  g_stage = "home-second-account";
  press(InputManager::BTN_DOWN);

  // --- Let a code rollover happen (30 s later) ---
  g_stage = "home-rollover";
  delay(31000);
  runLoops(3);

  // --- Sleep via power button; device locks ---
  g_stage = "sleep-locked";
  press(InputManager::BTN_POWER);
  expect(hostDeepSleepEntered, "power button enters deep sleep");

  // --- Wrong PIN: 111111 ---
  g_stage = "unlock-wrong-pin";
  for (int i = 0; i < 6; i++) {
    press(InputManager::BTN_UP);       // digit -> '1'
    press(InputManager::BTN_CONFIRM);  // commit
  }
  runLoops(2);
  expect(Preferences::store.count("fails") == 1, "wrong PIN recorded");

  // --- Correct PIN unlocks ---
  g_stage = "unlock-ok";
  for (int i = 0; i < 6; i++) press(InputManager::BTN_CONFIRM);
  runLoops(3);

  printf("\nSerial transcript:\n%s\n", Serial.tx.c_str());
  printf("%s (%d failure%s), %d frames written to %s/\n", g_failures == 0 ? "SIMULATION PASSED" : "SIMULATION FAILED",
         g_failures, g_failures == 1 ? "" : "s", g_frameCounter, g_outDir.c_str());
  return g_failures == 0 ? 0 : 1;
}
