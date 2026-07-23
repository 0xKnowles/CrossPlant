#include "Vault.h"

#include <Preferences.h>
#include <esp_random.h>
#include <mbedtls/gcm.h>

#include <cstring>

#include "Sha.h"

namespace inkkey {

namespace {

constexpr char NVS_NAMESPACE[] = "inkkey";
constexpr char KEY_VAULT[] = "vault";
constexpr char KEY_FAILS[] = "fails";
constexpr uint8_t MAGIC[4] = {'I', 'K', 'V', '1'};

// Packed on-disk record size (see packAccount).
constexpr size_t RECORD_SIZE = Account::MAX_LABEL + Account::MAX_ISSUER + Account::MAX_SECRET + 1 + 1 + 2 + 1;
constexpr size_t HEADER_SIZE = 4 + 4 + 16 + 12 + 2;
constexpr size_t TAG_SIZE = 16;
constexpr size_t MAX_PLAINTEXT = 1 + Vault::MAX_ACCOUNTS * RECORD_SIZE;
constexpr size_t MAX_BLOB = HEADER_SIZE + MAX_PLAINTEXT + TAG_SIZE;

void packAccount(const Account& a, uint8_t* out) {
  memcpy(out, a.label, Account::MAX_LABEL);
  out += Account::MAX_LABEL;
  memcpy(out, a.issuer, Account::MAX_ISSUER);
  out += Account::MAX_ISSUER;
  memcpy(out, a.secret, Account::MAX_SECRET);
  out += Account::MAX_SECRET;
  *out++ = a.secretLen;
  *out++ = a.digits;
  *out++ = uint8_t(a.period & 0xFF);
  *out++ = uint8_t(a.period >> 8);
  *out++ = uint8_t(a.algo);
}

void unpackAccount(const uint8_t* in, Account& a) {
  memcpy(a.label, in, Account::MAX_LABEL);
  a.label[Account::MAX_LABEL - 1] = '\0';
  in += Account::MAX_LABEL;
  memcpy(a.issuer, in, Account::MAX_ISSUER);
  a.issuer[Account::MAX_ISSUER - 1] = '\0';
  in += Account::MAX_ISSUER;
  memcpy(a.secret, in, Account::MAX_SECRET);
  in += Account::MAX_SECRET;
  a.secretLen = *in++;
  if (a.secretLen > Account::MAX_SECRET) a.secretLen = 0;
  a.digits = *in++;
  if (a.digits != 6 && a.digits != 8) a.digits = 6;
  a.period = uint16_t(in[0]) | (uint16_t(in[1]) << 8);
  in += 2;
  if (a.period < 5 || a.period > 300) a.period = 30;
  a.algo = (*in == uint8_t(TotpAlgo::Sha256)) ? TotpAlgo::Sha256 : TotpAlgo::Sha1;
}

bool gcmCrypt(bool encrypt, const uint8_t key[32], const uint8_t nonce[12], const uint8_t* in, size_t len,
              uint8_t* out, uint8_t tag[TAG_SIZE]) {
  mbedtls_gcm_context gcm;
  mbedtls_gcm_init(&gcm);
  bool ok = false;
  if (mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256) == 0) {
    if (encrypt) {
      ok = mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT, len, nonce, 12, nullptr, 0, in, out, TAG_SIZE,
                                     tag) == 0;
    } else {
      ok = mbedtls_gcm_auth_decrypt(&gcm, len, nonce, 12, nullptr, 0, tag, TAG_SIZE, in, out) == 0;
    }
  }
  mbedtls_gcm_free(&gcm);
  return ok;
}

}  // namespace

void Vault::deriveKey(const char* pin, const uint8_t* salt, uint8_t* keyOut) const {
  pbkdf2HmacSha256(reinterpret_cast<const uint8_t*>(pin), PIN_LEN, salt, 16, iterations_, keyOut, 32);
}

bool Vault::exists() const {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, /*readOnly=*/true)) return false;
  bool has = prefs.isKey(KEY_VAULT);
  prefs.end();
  return has;
}

bool Vault::create(const char* pin) {
  count_ = 0;
  iterations_ = KDF_ITERATIONS;
  esp_fill_random(salt_, sizeof(salt_));
  deriveKey(pin, salt_, key_);
  unlocked_ = true;
  clearFailedAttempts();
  return encryptAndStore();
}

Vault::UnlockResult Vault::unlock(const char* pin) {
  uint8_t blob[MAX_BLOB];
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, /*readOnly=*/true)) return UnlockResult::StorageError;
  size_t blobLen = prefs.getBytes(KEY_VAULT, blob, sizeof(blob));
  prefs.end();
  if (blobLen == 0) return UnlockResult::NoVault;
  if (blobLen < HEADER_SIZE + TAG_SIZE || memcmp(blob, MAGIC, 4) != 0) return UnlockResult::StorageError;

  const uint8_t* p = blob + 4;
  uint32_t iterations = uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
  p += 4;
  const uint8_t* salt = p;
  p += 16;
  const uint8_t* nonce = p;
  p += 12;
  uint16_t ctLen = uint16_t(p[0]) | (uint16_t(p[1]) << 8);
  p += 2;
  if (ctLen > MAX_PLAINTEXT || HEADER_SIZE + ctLen + TAG_SIZE != blobLen) return UnlockResult::StorageError;
  const uint8_t* ciphertext = p;
  uint8_t tag[TAG_SIZE];
  memcpy(tag, p + ctLen, TAG_SIZE);

  iterations_ = iterations;
  uint8_t key[32];
  deriveKey(pin, salt, key);

  uint8_t plain[MAX_PLAINTEXT];
  if (!gcmCrypt(false, key, nonce, ciphertext, ctLen, plain, tag)) {
    memset(key, 0, sizeof(key));
    return UnlockResult::WrongPin;
  }

  uint8_t n = plain[0];
  if (n > MAX_ACCOUNTS || 1 + size_t(n) * RECORD_SIZE != ctLen) {
    memset(key, 0, sizeof(key));
    memset(plain, 0, sizeof(plain));
    return UnlockResult::StorageError;
  }
  for (uint8_t i = 0; i < n; i++) unpackAccount(plain + 1 + i * RECORD_SIZE, accounts_[i]);
  count_ = n;
  memcpy(key_, key, sizeof(key_));
  memcpy(salt_, salt, sizeof(salt_));
  unlocked_ = true;
  memset(key, 0, sizeof(key));
  memset(plain, 0, sizeof(plain));
  clearFailedAttempts();
  return UnlockResult::Ok;
}

bool Vault::encryptAndStore() {
  if (!unlocked_) return false;

  uint8_t plain[MAX_PLAINTEXT];
  plain[0] = count_;
  for (uint8_t i = 0; i < count_; i++) packAccount(accounts_[i], plain + 1 + i * RECORD_SIZE);
  uint16_t ctLen = uint16_t(1 + size_t(count_) * RECORD_SIZE);

  uint8_t blob[MAX_BLOB];
  uint8_t* p = blob;
  memcpy(p, MAGIC, 4);
  p += 4;
  p[0] = uint8_t(iterations_);
  p[1] = uint8_t(iterations_ >> 8);
  p[2] = uint8_t(iterations_ >> 16);
  p[3] = uint8_t(iterations_ >> 24);
  p += 4;
  memcpy(p, salt_, 16);
  p += 16;
  uint8_t nonce[12];
  esp_fill_random(nonce, sizeof(nonce));
  memcpy(p, nonce, 12);
  p += 12;
  p[0] = uint8_t(ctLen & 0xFF);
  p[1] = uint8_t(ctLen >> 8);
  p += 2;

  uint8_t tag[TAG_SIZE];
  if (!gcmCrypt(true, key_, nonce, plain, ctLen, p, tag)) {
    memset(plain, 0, sizeof(plain));
    return false;
  }
  memcpy(p + ctLen, tag, TAG_SIZE);
  size_t blobLen = HEADER_SIZE + ctLen + TAG_SIZE;
  memset(plain, 0, sizeof(plain));

  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, /*readOnly=*/false)) return false;
  size_t written = prefs.putBytes(KEY_VAULT, blob, blobLen);
  prefs.end();
  return written == blobLen;
}

bool Vault::save() { return encryptAndStore(); }

bool Vault::changePin(const char* newPin) {
  if (!unlocked_) return false;
  esp_fill_random(salt_, sizeof(salt_));
  iterations_ = KDF_ITERATIONS;
  deriveKey(newPin, salt_, key_);
  return encryptAndStore();
}

void Vault::lock() {
  memset(key_, 0, sizeof(key_));
  memset(accounts_, 0, sizeof(accounts_));
  count_ = 0;
  unlocked_ = false;
}

void Vault::wipe() {
  lock();
  Preferences prefs;
  if (prefs.begin(NVS_NAMESPACE, /*readOnly=*/false)) {
    prefs.clear();
    prefs.end();
  }
}

bool Vault::addAccount(const Account& a) {
  if (!unlocked_ || count_ >= MAX_ACCOUNTS) return false;
  // Insert sorted by issuer, then label (case-sensitive is fine for a display order).
  uint8_t pos = count_;
  for (uint8_t i = 0; i < count_; i++) {
    int c = strcmp(a.issuer, accounts_[i].issuer);
    if (c < 0 || (c == 0 && strcmp(a.label, accounts_[i].label) < 0)) {
      pos = i;
      break;
    }
  }
  for (uint8_t i = count_; i > pos; i--) accounts_[i] = accounts_[i - 1];
  accounts_[pos] = a;
  count_++;
  return true;
}

bool Vault::removeAccount(uint8_t index) {
  if (!unlocked_ || index >= count_) return false;
  for (uint8_t i = index; i + 1 < count_; i++) accounts_[i] = accounts_[i + 1];
  count_--;
  memset(&accounts_[count_], 0, sizeof(Account));
  return true;
}

uint32_t Vault::failedAttempts() const {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, /*readOnly=*/true)) return 0;
  uint32_t fails = prefs.getUInt(KEY_FAILS, 0);
  prefs.end();
  return fails;
}

void Vault::recordFailedAttempt() {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, /*readOnly=*/false)) return;
  prefs.putUInt(KEY_FAILS, prefs.getUInt(KEY_FAILS, 0) + 1);
  prefs.end();
}

void Vault::clearFailedAttempts() {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, /*readOnly=*/false)) return;
  if (prefs.getUInt(KEY_FAILS, 0) != 0) prefs.putUInt(KEY_FAILS, 0);
  prefs.end();
}

uint32_t Vault::backoffSeconds() const {
  uint32_t fails = failedAttempts();
  if (fails < 3) return 0;
  if (fails >= 12) return 900;  // 15 min cap
  // 3 fails -> 30s, then doubling: 30, 60, 120, 240, 480, 900...
  uint32_t s = 30u << (fails - 3);
  return s > 900 ? 900 : s;
}

}  // namespace inkkey
