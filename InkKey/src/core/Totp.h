#pragma once

// RFC 4226 (HOTP) / RFC 6238 (TOTP) code generation, base32 decoding, and
// otpauth:// URI parsing. Freestanding C++ — host-testable against the RFC
// vectors (see test/test_totp.cpp).

#include <cstddef>
#include <cstdint>

namespace inkkey {

enum class TotpAlgo : uint8_t { Sha1 = 0, Sha256 = 1 };

struct Account {
  static constexpr size_t MAX_LABEL = 40;
  static constexpr size_t MAX_ISSUER = 24;
  static constexpr size_t MAX_SECRET = 40;  // decoded bytes (supports up to 64-char base32)

  char label[MAX_LABEL] = {0};
  char issuer[MAX_ISSUER] = {0};
  uint8_t secret[MAX_SECRET] = {0};
  uint8_t secretLen = 0;
  uint8_t digits = 6;   // 6 or 8
  uint16_t period = 30;  // seconds
  TotpAlgo algo = TotpAlgo::Sha1;
};

// Decodes RFC 4648 base32 (case-insensitive, '=' padding and spaces ignored).
// Returns decoded length, or -1 on invalid input or overflow of outCap.
int base32Decode(const char* in, uint8_t* out, size_t outCap);

// RFC 4226 HOTP. digits must be 6..8.
uint32_t hotp(const uint8_t* secret, size_t secretLen, uint64_t counter, uint8_t digits, TotpAlgo algo);

// RFC 6238 TOTP for the given unix time (UTC seconds).
uint32_t totp(const Account& account, uint64_t unixTime);

// Seconds remaining until the current code rolls over.
uint32_t totpSecondsRemaining(const Account& account, uint64_t unixTime);

// Formats a code as a NUL-terminated string with a space between digit groups
// ("123 456", "1234 5678"). buf must hold at least 10 bytes.
void formatCode(uint32_t code, uint8_t digits, char* buf);

// Parses an otpauth://totp/... URI into `out`. Returns nullptr on success or a
// static human-readable error string on failure. HOTP (counter-based) URIs are
// rejected — this device has no counter sync story by design.
const char* parseOtpauthUri(const char* uri, Account& out);

}  // namespace inkkey
