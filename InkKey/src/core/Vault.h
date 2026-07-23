#pragma once

// Encrypted account vault, stored as a single blob in NVS on internal flash.
//
// Layout of the blob (little-endian):
//   magic     "IKV1"                     4 bytes
//   iterations u32 (PBKDF2 rounds)       4 bytes
//   salt                                16 bytes
//   nonce (fresh random per save)       12 bytes
//   ctLen     u16                        2 bytes
//   ciphertext                      ctLen bytes
//   tag (AES-256-GCM)                   16 bytes
//
// Key = PBKDF2-HMAC-SHA256(pin, salt, iterations). The plaintext is a packed
// array of Account records preceded by a count byte. Decryption failure (bad
// PIN or tampered blob) is indistinguishable by design: GCM tag mismatch.
//
// The decrypted accounts and derived key live only in RAM; deep sleep powers
// the RAM down, so every wake starts locked.

#include <cstddef>
#include <cstdint>

#include "Totp.h"

namespace inkkey {

class Vault {
 public:
  static constexpr size_t MAX_ACCOUNTS = 24;
  static constexpr uint32_t KDF_ITERATIONS = 20000;
  static constexpr size_t PIN_LEN = 6;

  enum class UnlockResult : uint8_t { Ok, WrongPin, NoVault, StorageError };

  // True once a vault blob exists in NVS (device has been set up).
  bool exists() const;

  // Creates a fresh empty vault protected by `pin` (PIN_LEN digit chars).
  // Overwrites any existing vault. Leaves the vault unlocked.
  bool create(const char* pin);

  // Attempts decryption with `pin`. On success the account list is in RAM and
  // isUnlocked() is true.
  UnlockResult unlock(const char* pin);

  // Re-encrypts with a fresh nonce and writes to NVS. Requires unlocked.
  bool save();

  // Changes the PIN: fresh salt, fresh key, fresh nonce. Requires unlocked.
  bool changePin(const char* newPin);

  // Drops the key and accounts from RAM.
  void lock();

  // Destroys the vault blob and the failed-attempt counter in NVS.
  void wipe();

  bool isUnlocked() const { return unlocked_; }
  uint8_t accountCount() const { return count_; }
  const Account& account(uint8_t i) const { return accounts_[i]; }

  // Returns false when full. Keeps accounts sorted by issuer/label.
  bool addAccount(const Account& a);
  bool removeAccount(uint8_t index);

  // --- wrong-PIN backoff (stored in NVS, survives reboot/sleep) ---
  uint32_t failedAttempts() const;
  void recordFailedAttempt();
  void clearFailedAttempts();
  // Seconds the UI must wait before allowing another attempt (0 = none).
  uint32_t backoffSeconds() const;

 private:
  bool encryptAndStore();
  void deriveKey(const char* pin, const uint8_t* salt, uint8_t* keyOut) const;

  Account accounts_[MAX_ACCOUNTS];
  uint8_t count_ = 0;
  bool unlocked_ = false;
  uint8_t key_[32] = {0};
  uint8_t salt_[16] = {0};
  uint32_t iterations_ = KDF_ITERATIONS;
};

}  // namespace inkkey
