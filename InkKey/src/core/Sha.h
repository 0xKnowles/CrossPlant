#pragma once

// Self-contained SHA-1 / SHA-256 + HMAC + PBKDF2-HMAC-SHA256.
//
// Freestanding C++ (no Arduino/ESP-IDF dependency) so the TOTP and KDF code
// paths can be verified with host-side unit tests against the RFC vectors.
// Constant-time enough for this use: inputs are secrets of fixed small sizes,
// and the device is single-user with no remote attacker measuring timing.

#include <cstddef>
#include <cstdint>

namespace inkkey {

struct Sha1 {
  static constexpr size_t DIGEST_LEN = 20;
  static constexpr size_t BLOCK_LEN = 64;

  void begin();
  void update(const uint8_t* data, size_t len);
  void finish(uint8_t out[DIGEST_LEN]);

 private:
  void processBlock(const uint8_t block[64]);
  uint32_t h_[5];
  uint64_t totalLen_ = 0;
  uint8_t buf_[64];
  size_t bufLen_ = 0;
};

struct Sha256 {
  static constexpr size_t DIGEST_LEN = 32;
  static constexpr size_t BLOCK_LEN = 64;

  void begin();
  void update(const uint8_t* data, size_t len);
  void finish(uint8_t out[DIGEST_LEN]);

 private:
  void processBlock(const uint8_t block[64]);
  uint32_t h_[8];
  uint64_t totalLen_ = 0;
  uint8_t buf_[64];
  size_t bufLen_ = 0;
};

// out must hold 20 bytes.
void hmacSha1(const uint8_t* key, size_t keyLen, const uint8_t* msg, size_t msgLen, uint8_t* out);

// out must hold 32 bytes.
void hmacSha256(const uint8_t* key, size_t keyLen, const uint8_t* msg, size_t msgLen, uint8_t* out);

// PBKDF2-HMAC-SHA256 (RFC 2898 / RFC 8018).
void pbkdf2HmacSha256(const uint8_t* password, size_t passwordLen, const uint8_t* salt, size_t saltLen,
                      uint32_t iterations, uint8_t* out, size_t outLen);

// Constant-time comparison; returns true when equal.
bool constantTimeEquals(const uint8_t* a, const uint8_t* b, size_t len);

}  // namespace inkkey
