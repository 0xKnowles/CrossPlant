#include "Totp.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>

#include "Sha.h"

namespace inkkey {

int base32Decode(const char* in, uint8_t* out, size_t outCap) {
  uint32_t bits = 0;
  int bitCount = 0;
  size_t outLen = 0;
  for (const char* p = in; *p; p++) {
    char c = *p;
    if (c == ' ' || c == '-') continue;  // common visual grouping
    if (c == '=') break;                 // padding: done
    int v;
    if (c >= 'A' && c <= 'Z') {
      v = c - 'A';
    } else if (c >= 'a' && c <= 'z') {
      v = c - 'a';
    } else if (c >= '2' && c <= '7') {
      v = c - '2' + 26;
    } else {
      return -1;
    }
    bits = (bits << 5) | uint32_t(v);
    bitCount += 5;
    if (bitCount >= 8) {
      if (outLen >= outCap) return -1;
      out[outLen++] = uint8_t(bits >> (bitCount - 8));
      bitCount -= 8;
    }
  }
  return int(outLen);
}

uint32_t hotp(const uint8_t* secret, size_t secretLen, uint64_t counter, uint8_t digits, TotpAlgo algo) {
  uint8_t msg[8];
  for (int i = 0; i < 8; i++) msg[i] = uint8_t(counter >> (56 - i * 8));

  uint8_t mac[32];
  size_t macLen;
  if (algo == TotpAlgo::Sha256) {
    hmacSha256(secret, secretLen, msg, sizeof(msg), mac);
    macLen = 32;
  } else {
    hmacSha1(secret, secretLen, msg, sizeof(msg), mac);
    macLen = 20;
  }

  // RFC 4226 dynamic truncation.
  size_t offset = mac[macLen - 1] & 0x0F;
  uint32_t binCode = (uint32_t(mac[offset] & 0x7F) << 24) | (uint32_t(mac[offset + 1]) << 16) |
                     (uint32_t(mac[offset + 2]) << 8) | uint32_t(mac[offset + 3]);

  uint32_t mod = 1;
  for (uint8_t i = 0; i < digits; i++) mod *= 10;
  return binCode % mod;
}

uint32_t totp(const Account& account, uint64_t unixTime) {
  uint16_t period = account.period ? account.period : 30;
  return hotp(account.secret, account.secretLen, unixTime / period, account.digits, account.algo);
}

uint32_t totpSecondsRemaining(const Account& account, uint64_t unixTime) {
  uint16_t period = account.period ? account.period : 30;
  return period - uint32_t(unixTime % period);
}

void formatCode(uint32_t code, uint8_t digits, char* buf) {
  if (digits < 6 || digits > 8) digits = 6;
  char raw[12];
  snprintf(raw, sizeof(raw), "%0*lu", int(digits), static_cast<unsigned long>(code));
  size_t firstGroup = (digits == 6) ? 3 : digits / 2;
  size_t o = 0;
  for (size_t i = 0; i < digits; i++) {
    if (i == firstGroup) buf[o++] = ' ';
    buf[o++] = raw[i];
  }
  buf[o] = '\0';
}

// --- otpauth:// parsing ------------------------------------------------------

namespace {

// In-place %XX decoding; also turns '+' into a space.
void urlDecode(char* s) {
  char* w = s;
  for (char* r = s; *r; r++) {
    if (*r == '%' && r[1] && r[2]) {
      auto hex = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
      };
      int hi = hex(r[1]), lo = hex(r[2]);
      if (hi >= 0 && lo >= 0) {
        *w++ = char(hi * 16 + lo);
        r += 2;
        continue;
      }
    }
    *w++ = (*r == '+') ? ' ' : *r;
  }
  *w = '\0';
}

void copyTruncated(char* dst, size_t dstCap, const char* src) {
  size_t n = strlen(src);
  if (n >= dstCap) n = dstCap - 1;
  memcpy(dst, src, n);
  dst[n] = '\0';
}

}  // namespace

const char* parseOtpauthUri(const char* uri, Account& out) {
  static constexpr size_t MAX_URI = 512;
  char work[MAX_URI];
  if (strlen(uri) >= sizeof(work)) return "URI too long";
  strcpy(work, uri);

  if (strncmp(work, "otpauth://hotp/", 15) == 0) return "HOTP (counter-based) accounts are not supported";
  if (strncmp(work, "otpauth://totp/", 15) != 0) return "not an otpauth://totp/ URI";

  char* label = work + 15;
  char* query = strchr(label, '?');
  if (query) *query++ = '\0';

  urlDecode(label);

  out = Account{};

  // Label may be "Issuer:account". The issuer query param (below) wins if both exist.
  char* colon = strchr(label, ':');
  if (colon) {
    *colon = '\0';
    copyTruncated(out.issuer, sizeof(out.issuer), label);
    copyTruncated(out.label, sizeof(out.label), colon + 1);
  } else {
    copyTruncated(out.label, sizeof(out.label), label);
  }

  bool haveSecret = false;
  while (query && *query) {
    char* next = strchr(query, '&');
    if (next) *next++ = '\0';
    char* eq = strchr(query, '=');
    if (eq) {
      *eq = '\0';
      char* value = eq + 1;
      urlDecode(value);
      if (strcmp(query, "secret") == 0) {
        int n = base32Decode(value, out.secret, sizeof(out.secret));
        if (n <= 0) return "secret is not valid base32 (or longer than 64 chars)";
        out.secretLen = uint8_t(n);
        haveSecret = true;
      } else if (strcmp(query, "issuer") == 0) {
        copyTruncated(out.issuer, sizeof(out.issuer), value);
      } else if (strcmp(query, "digits") == 0) {
        int d = atoi(value);
        if (d != 6 && d != 8) return "digits must be 6 or 8";
        out.digits = uint8_t(d);
      } else if (strcmp(query, "period") == 0) {
        int p = atoi(value);
        if (p < 5 || p > 300) return "period must be 5..300 seconds";
        out.period = uint16_t(p);
      } else if (strcmp(query, "algorithm") == 0) {
        if (strcasecmp(value, "SHA1") == 0) {
          out.algo = TotpAlgo::Sha1;
        } else if (strcasecmp(value, "SHA256") == 0) {
          out.algo = TotpAlgo::Sha256;
        } else {
          return "algorithm must be SHA1 or SHA256";
        }
      }
      // Unknown params (counter, image, ...) are ignored.
    }
    query = next;
  }

  if (!haveSecret) return "URI has no secret parameter";
  if (out.label[0] == '\0' && out.issuer[0] == '\0') return "URI has no account label";
  return nullptr;
}

}  // namespace inkkey
