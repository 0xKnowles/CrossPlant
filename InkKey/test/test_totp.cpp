// Host-side verification of InkKey's crypto and TOTP primitives against the
// published RFC test vectors. Build and run:
//
//   g++ -std=c++17 -I../src -o test_totp test_totp.cpp ../src/core/Sha.cpp ../src/core/Totp.cpp && ./test_totp
//
// Exits non-zero on the first failure.

#include <cstdio>
#include <cstring>

#include "core/Sha.h"
#include "core/Totp.h"

using namespace inkkey;

static int failures = 0;

static void check(bool ok, const char* what) {
  printf("%s %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) failures++;
}

static bool hexEquals(const uint8_t* got, const char* expectedHex, size_t len) {
  for (size_t i = 0; i < len; i++) {
    char buf[3];
    snprintf(buf, sizeof(buf), "%02x", got[i]);
    if (buf[0] != expectedHex[i * 2] || buf[1] != expectedHex[i * 2 + 1]) return false;
  }
  return true;
}

int main() {
  // --- SHA-1 (FIPS 180-4 "abc") ---
  {
    Sha1 h;
    h.begin();
    h.update(reinterpret_cast<const uint8_t*>("abc"), 3);
    uint8_t d[20];
    h.finish(d);
    check(hexEquals(d, "a9993e364706816aba3e25717850c26c9cd0d89d", 20), "SHA-1(abc)");
  }

  // --- SHA-256 (FIPS 180-4 "abc") ---
  {
    Sha256 h;
    h.begin();
    h.update(reinterpret_cast<const uint8_t*>("abc"), 3);
    uint8_t d[32];
    h.finish(d);
    check(hexEquals(d, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", 32), "SHA-256(abc)");
  }

  // --- HMAC-SHA1 (RFC 2202 test case 1) ---
  {
    uint8_t key[20];
    memset(key, 0x0b, sizeof(key));
    uint8_t mac[20];
    hmacSha1(key, sizeof(key), reinterpret_cast<const uint8_t*>("Hi There"), 8, mac);
    check(hexEquals(mac, "b617318655057264e28bc0b6fb378c8ef146be00", 20), "HMAC-SHA1 RFC2202 TC1");
  }

  // --- HMAC-SHA256 (RFC 4231 test case 1) ---
  {
    uint8_t key[20];
    memset(key, 0x0b, sizeof(key));
    uint8_t mac[32];
    hmacSha256(key, sizeof(key), reinterpret_cast<const uint8_t*>("Hi There"), 8, mac);
    check(hexEquals(mac, "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7", 32),
          "HMAC-SHA256 RFC4231 TC1");
  }

  // --- PBKDF2-HMAC-SHA256 (RFC 8018-style published vectors) ---
  {
    uint8_t dk[40];
    pbkdf2HmacSha256(reinterpret_cast<const uint8_t*>("password"), 8, reinterpret_cast<const uint8_t*>("salt"), 4, 1,
                     dk, 32);
    check(hexEquals(dk, "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b", 32), "PBKDF2 c=1");

    pbkdf2HmacSha256(reinterpret_cast<const uint8_t*>("password"), 8, reinterpret_cast<const uint8_t*>("salt"), 4,
                     4096, dk, 32);
    check(hexEquals(dk, "c5e478d59288c841aa530db6845c4c8d962893a001ce4e11a4963873aa98134a", 32), "PBKDF2 c=4096");

    pbkdf2HmacSha256(reinterpret_cast<const uint8_t*>("passwordPASSWORDpassword"), 24,
                     reinterpret_cast<const uint8_t*>("saltSALTsaltSALTsaltSALTsaltSALTsalt"), 36, 4096, dk, 40);
    check(hexEquals(dk, "348c89dbcbd32b2f32d814b8116e84cf2b17347ebc1800181c4e2a1fb8dd53e1c635518c7dac47e9", 40),
          "PBKDF2 multi-block dkLen=40");
  }

  // --- base32 ---
  {
    uint8_t out[64];
    int n = base32Decode("GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ", out, sizeof(out));
    check(n == 20 && memcmp(out, "12345678901234567890", 20) == 0, "base32 decode");
    n = base32Decode("gezdgnbvgy3tqojqgezdgnbvgy3tqojq", out, sizeof(out));
    check(n == 20 && memcmp(out, "12345678901234567890", 20) == 0, "base32 decode lowercase");
    n = base32Decode("MFRGG===", out, sizeof(out));
    check(n == 3 && memcmp(out, "abc", 3) == 0, "base32 padding");
    check(base32Decode("1nvalid!", out, sizeof(out)) == -1, "base32 rejects invalid chars");
  }

  // --- HOTP (RFC 4226 Appendix D) ---
  {
    const uint8_t* secret = reinterpret_cast<const uint8_t*>("12345678901234567890");
    const uint32_t expected[10] = {755224, 287082, 359152, 969429, 338314, 254676, 287922, 162583, 399871, 520489};
    bool ok = true;
    for (uint64_t c = 0; c < 10; c++) {
      if (hotp(secret, 20, c, 6, TotpAlgo::Sha1) != expected[c]) ok = false;
    }
    check(ok, "HOTP RFC4226 Appendix D (10 vectors)");
  }

  // --- TOTP (RFC 6238 Appendix B) ---
  {
    Account a;
    memcpy(a.secret, "12345678901234567890", 20);
    a.secretLen = 20;
    a.digits = 8;
    a.period = 30;
    a.algo = TotpAlgo::Sha1;
    const uint64_t times[6] = {59, 1111111109, 1111111111, 1234567890, 2000000000, 20000000000ULL};
    const uint32_t sha1Expected[6] = {94287082, 7081804, 14050471, 89005924, 69279037, 65353130};
    bool ok = true;
    for (int i = 0; i < 6; i++) {
      if (totp(a, times[i]) != sha1Expected[i]) ok = false;
    }
    check(ok, "TOTP RFC6238 SHA1 (6 vectors)");

    Account b;
    memcpy(b.secret, "12345678901234567890123456789012", 32);
    b.secretLen = 32;
    b.digits = 8;
    b.period = 30;
    b.algo = TotpAlgo::Sha256;
    const uint32_t sha256Expected[6] = {46119246, 68084774, 67062674, 91819424, 90698825, 77737706};
    ok = true;
    for (int i = 0; i < 6; i++) {
      if (totp(b, times[i]) != sha256Expected[i]) ok = false;
    }
    check(ok, "TOTP RFC6238 SHA256 (6 vectors)");
  }

  // --- otpauth parsing ---
  {
    Account a;
    const char* err = parseOtpauthUri(
        "otpauth://totp/GitHub:alice%40example.com?secret=GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ&issuer=GitHub&digits=6&period=30",
        a);
    check(err == nullptr && strcmp(a.issuer, "GitHub") == 0 && strcmp(a.label, "alice@example.com") == 0 &&
              a.secretLen == 20 && a.digits == 6 && a.period == 30 && a.algo == TotpAlgo::Sha1,
          "otpauth full URI");

    err = parseOtpauthUri("otpauth://totp/plain?secret=MFRGG&algorithm=SHA256&digits=8", a);
    check(err == nullptr && a.digits == 8 && a.algo == TotpAlgo::Sha256 && strcmp(a.label, "plain") == 0,
          "otpauth minimal URI");

    check(parseOtpauthUri("otpauth://hotp/x?secret=MFRGG", a) != nullptr, "otpauth rejects HOTP");
    check(parseOtpauthUri("otpauth://totp/x", a) != nullptr, "otpauth rejects missing secret");
    check(parseOtpauthUri("otpauth://totp/x?secret=!!!", a) != nullptr, "otpauth rejects bad secret");
    check(parseOtpauthUri("otpauth://totp/x?secret=MFRGG&algorithm=SHA512", a) != nullptr,
          "otpauth rejects SHA512");
  }

  // --- formatting + rollover ---
  {
    char buf[10];
    formatCode(755224, 6, buf);
    check(strcmp(buf, "755 224") == 0, "formatCode 6 digits");
    formatCode(94287082, 8, buf);
    check(strcmp(buf, "9428 7082") == 0, "formatCode 8 digits");
    formatCode(42, 6, buf);
    check(strcmp(buf, "000 042") == 0, "formatCode zero-pads");

    Account a;
    a.period = 30;
    check(totpSecondsRemaining(a, 59) == 1 && totpSecondsRemaining(a, 60) == 30, "seconds remaining");
  }

  printf("\n%s (%d failure%s)\n", failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED", failures,
         failures == 1 ? "" : "s");
  return failures == 0 ? 0 : 1;
}
