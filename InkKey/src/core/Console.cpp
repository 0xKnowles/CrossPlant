#include "Console.h"

#include <Arduino.h>

#include <cstdlib>
#include <cstring>

#include "Clock.h"
#include "Totp.h"
#include "Vault.h"

namespace inkkey {

void Console::begin(Vault* vault, Clock* clock) {
  vault_ = vault;
  clock_ = clock;
  lineLen_ = 0;
  done_ = false;
}

void Console::drain() {
  while (Serial.available()) Serial.read();
  lineLen_ = 0;
}

bool Console::doneRequested() {
  bool d = done_;
  done_ = false;
  return d;
}

bool Console::service() {
  dirty_ = false;
  while (Serial.available()) {
    char c = char(Serial.read());
    if (c == '\r') continue;
    if (c == '\n') {
      line_[lineLen_] = '\0';
      if (lineLen_ > 0) handleLine(line_);
      lineLen_ = 0;
      continue;
    }
    if (lineLen_ < sizeof(line_) - 1) {
      line_[lineLen_++] = c;
    } else {
      lineLen_ = 0;  // overlong line: drop it entirely
      Serial.println("ERR line too long");
    }
  }
  return dirty_;
}

void Console::printHelp() {
  Serial.println("InkKey provisioning console");
  Serial.println("  time                       show RTC time (UTC)");
  Serial.println("  time YYYY-MM-DDTHH:MM:SSZ  set RTC (UTC)");
  Serial.println("  add otpauth://totp/...     add account");
  Serial.println("  list                       list accounts");
  Serial.println("  del N                      delete account N");
  Serial.println("  done                       leave provisioning mode");
}

void Console::handleLine(char* line) {
  if (strcmp(line, "help") == 0) {
    printHelp();
    return;
  }

  if (strcmp(line, "done") == 0) {
    done_ = true;
    Serial.println("OK leaving provisioning mode");
    return;
  }

  if (strcmp(line, "time") == 0) {
    char buf[32];
    clock_->formatUtc(buf, sizeof(buf));
    Serial.print("RTC: ");
    Serial.println(buf);
    if (!clock_->timeValid()) Serial.println("WARN time not set - codes will be wrong until you set it");
    return;
  }

  if (strncmp(line, "time ", 5) == 0) {
    unsigned y, mo, d, h, mi, s;
    if (sscanf(line + 5, "%4u-%2u-%2u%*[T ]%2u:%2u:%2u", &y, &mo, &d, &h, &mi, &s) != 6 || y < 2025 || y > 2099 ||
        mo < 1 || mo > 12 || d < 1 || d > 31 || h > 23 || mi > 59 || s > 59) {
      Serial.println("ERR expected time YYYY-MM-DDTHH:MM:SSZ (UTC)");
      return;
    }
    if (!clock_->setUtc(uint16_t(y), uint8_t(mo), uint8_t(d), uint8_t(h), uint8_t(mi), uint8_t(s))) {
      Serial.println("ERR RTC write failed");
      return;
    }
    char buf[32];
    clock_->formatUtc(buf, sizeof(buf));
    Serial.print("OK RTC set to ");
    Serial.println(buf);
    dirty_ = true;
    return;
  }

  if (strncmp(line, "add ", 4) == 0) {
    Account a;
    const char* err = parseOtpauthUri(line + 4, a);
    if (err) {
      Serial.print("ERR ");
      Serial.println(err);
      return;
    }
    if (!vault_->addAccount(a)) {
      Serial.println("ERR vault full (24 accounts max)");
      return;
    }
    if (!vault_->save()) {
      Serial.println("ERR vault save failed");
      return;
    }
    Serial.print("OK added ");
    Serial.print(a.issuer[0] ? a.issuer : "(no issuer)");
    Serial.print(" / ");
    Serial.println(a.label);
    dirty_ = true;
    return;
  }

  if (strcmp(line, "list") == 0) {
    if (vault_->accountCount() == 0) {
      Serial.println("(no accounts)");
      return;
    }
    for (uint8_t i = 0; i < vault_->accountCount(); i++) {
      const Account& a = vault_->account(i);
      Serial.print(i);
      Serial.print(": ");
      Serial.print(a.issuer[0] ? a.issuer : "(no issuer)");
      Serial.print(" / ");
      Serial.print(a.label);
      Serial.print("  [");
      Serial.print(a.digits);
      Serial.print(" digits, ");
      Serial.print(a.period);
      Serial.print("s, ");
      Serial.print(a.algo == TotpAlgo::Sha256 ? "SHA256" : "SHA1");
      Serial.println("]");
    }
    return;
  }

  if (strncmp(line, "del ", 4) == 0) {
    int idx = atoi(line + 4);
    if (idx < 0 || idx >= vault_->accountCount()) {
      Serial.println("ERR no such account index (see `list`)");
      return;
    }
    if (!vault_->removeAccount(uint8_t(idx)) || !vault_->save()) {
      Serial.println("ERR delete failed");
      return;
    }
    Serial.println("OK deleted");
    dirty_ = true;
    return;
  }

  Serial.println("ERR unknown command (try `help`)");
}

}  // namespace inkkey
