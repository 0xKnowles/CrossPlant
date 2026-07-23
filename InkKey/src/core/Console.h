#pragma once

// USB-CDC provisioning console.
//
// Only serviced while the app is on the Provisioning screen with the vault
// unlocked — at every other moment serial input is drained and ignored, so a
// malicious USB host can't talk to a locked device. All commands echo their
// result; secrets are never printed back.
//
// Commands:
//   help                       command list
//   time                       show RTC time (UTC)
//   time YYYY-MM-DDTHH:MM:SSZ  set RTC (UTC; trailing Z optional)
//   add otpauth://totp/...     add an account from a standard otpauth URI
//   list                       accounts (index, issuer, label — no secrets)
//   del N                      delete account with index N (from `list`)
//   done                       leave provisioning mode

#include <cstdint>

namespace inkkey {

class Clock;
class Vault;

class Console {
 public:
  void begin(Vault* vault, Clock* clock);

  // Reads pending serial bytes and executes complete lines.
  // Returns true when any command changed vault/RTC state (UI should repaint).
  bool service();

  // Discards pending serial input (called while the console is not active).
  void drain();

  // True after a `done` command; the app polls and clears this to exit
  // provisioning mode.
  bool doneRequested();

 private:
  void handleLine(char* line);
  void printHelp();

  Vault* vault_ = nullptr;
  Clock* clock_ = nullptr;
  char line_[560];
  uint16_t lineLen_ = 0;
  bool done_ = false;
  bool dirty_ = false;
};

}  // namespace inkkey
