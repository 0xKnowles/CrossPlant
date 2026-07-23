#include "Sha.h"

#include <cstring>

namespace inkkey {

namespace {

inline uint32_t rotl(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }
inline uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

inline uint32_t loadBe32(const uint8_t* p) {
  return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}

inline void storeBe32(uint8_t* p, uint32_t v) {
  p[0] = uint8_t(v >> 24);
  p[1] = uint8_t(v >> 16);
  p[2] = uint8_t(v >> 8);
  p[3] = uint8_t(v);
}

}  // namespace

// --- SHA-1 (FIPS 180-4) ------------------------------------------------------

void Sha1::begin() {
  h_[0] = 0x67452301;
  h_[1] = 0xEFCDAB89;
  h_[2] = 0x98BADCFE;
  h_[3] = 0x10325476;
  h_[4] = 0xC3D2E1F0;
  totalLen_ = 0;
  bufLen_ = 0;
}

void Sha1::processBlock(const uint8_t block[64]) {
  uint32_t w[80];
  for (int i = 0; i < 16; i++) w[i] = loadBe32(block + i * 4);
  for (int i = 16; i < 80; i++) w[i] = rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

  uint32_t a = h_[0], b = h_[1], c = h_[2], d = h_[3], e = h_[4];
  for (int i = 0; i < 80; i++) {
    uint32_t f, k;
    if (i < 20) {
      f = (b & c) | ((~b) & d);
      k = 0x5A827999;
    } else if (i < 40) {
      f = b ^ c ^ d;
      k = 0x6ED9EBA1;
    } else if (i < 60) {
      f = (b & c) | (b & d) | (c & d);
      k = 0x8F1BBCDC;
    } else {
      f = b ^ c ^ d;
      k = 0xCA62C1D6;
    }
    uint32_t tmp = rotl(a, 5) + f + e + k + w[i];
    e = d;
    d = c;
    c = rotl(b, 30);
    b = a;
    a = tmp;
  }
  h_[0] += a;
  h_[1] += b;
  h_[2] += c;
  h_[3] += d;
  h_[4] += e;
}

void Sha1::update(const uint8_t* data, size_t len) {
  totalLen_ += len;
  while (len > 0) {
    size_t take = 64 - bufLen_;
    if (take > len) take = len;
    memcpy(buf_ + bufLen_, data, take);
    bufLen_ += take;
    data += take;
    len -= take;
    if (bufLen_ == 64) {
      processBlock(buf_);
      bufLen_ = 0;
    }
  }
}

void Sha1::finish(uint8_t out[DIGEST_LEN]) {
  uint64_t bitLen = totalLen_ * 8;
  uint8_t pad = 0x80;
  update(&pad, 1);
  uint8_t zero = 0x00;
  while (bufLen_ != 56) update(&zero, 1);
  uint8_t lenBytes[8];
  for (int i = 0; i < 8; i++) lenBytes[i] = uint8_t(bitLen >> (56 - i * 8));
  update(lenBytes, 8);
  for (int i = 0; i < 5; i++) storeBe32(out + i * 4, h_[i]);
}

// --- SHA-256 (FIPS 180-4) ----------------------------------------------------

namespace {
constexpr uint32_t kSha256K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
}  // namespace

void Sha256::begin() {
  h_[0] = 0x6a09e667;
  h_[1] = 0xbb67ae85;
  h_[2] = 0x3c6ef372;
  h_[3] = 0xa54ff53a;
  h_[4] = 0x510e527f;
  h_[5] = 0x9b05688c;
  h_[6] = 0x1f83d9ab;
  h_[7] = 0x5be0cd19;
  totalLen_ = 0;
  bufLen_ = 0;
}

void Sha256::processBlock(const uint8_t block[64]) {
  uint32_t w[64];
  for (int i = 0; i < 16; i++) w[i] = loadBe32(block + i * 4);
  for (int i = 16; i < 64; i++) {
    uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
    uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
  }

  uint32_t a = h_[0], b = h_[1], c = h_[2], d = h_[3];
  uint32_t e = h_[4], f = h_[5], g = h_[6], h = h_[7];
  for (int i = 0; i < 64; i++) {
    uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
    uint32_t ch = (e & f) ^ ((~e) & g);
    uint32_t t1 = h + s1 + ch + kSha256K[i] + w[i];
    uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
    uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
    uint32_t t2 = s0 + maj;
    h = g;
    g = f;
    f = e;
    e = d + t1;
    d = c;
    c = b;
    b = a;
    a = t1 + t2;
  }
  h_[0] += a;
  h_[1] += b;
  h_[2] += c;
  h_[3] += d;
  h_[4] += e;
  h_[5] += f;
  h_[6] += g;
  h_[7] += h;
}

void Sha256::update(const uint8_t* data, size_t len) {
  totalLen_ += len;
  while (len > 0) {
    size_t take = 64 - bufLen_;
    if (take > len) take = len;
    memcpy(buf_ + bufLen_, data, take);
    bufLen_ += take;
    data += take;
    len -= take;
    if (bufLen_ == 64) {
      processBlock(buf_);
      bufLen_ = 0;
    }
  }
}

void Sha256::finish(uint8_t out[DIGEST_LEN]) {
  uint64_t bitLen = totalLen_ * 8;
  uint8_t pad = 0x80;
  update(&pad, 1);
  uint8_t zero = 0x00;
  while (bufLen_ != 56) update(&zero, 1);
  uint8_t lenBytes[8];
  for (int i = 0; i < 8; i++) lenBytes[i] = uint8_t(bitLen >> (56 - i * 8));
  update(lenBytes, 8);
  for (int i = 0; i < 8; i++) storeBe32(out + i * 4, h_[i]);
}

// --- HMAC (RFC 2104) ---------------------------------------------------------

template <typename Hash>
static void hmac(const uint8_t* key, size_t keyLen, const uint8_t* msg, size_t msgLen, uint8_t* out) {
  uint8_t keyBlock[Hash::BLOCK_LEN];
  memset(keyBlock, 0, sizeof(keyBlock));
  if (keyLen > Hash::BLOCK_LEN) {
    Hash h;
    h.begin();
    h.update(key, keyLen);
    h.finish(keyBlock);
  } else {
    memcpy(keyBlock, key, keyLen);
  }

  uint8_t ipad[Hash::BLOCK_LEN], opad[Hash::BLOCK_LEN];
  for (size_t i = 0; i < Hash::BLOCK_LEN; i++) {
    ipad[i] = keyBlock[i] ^ 0x36;
    opad[i] = keyBlock[i] ^ 0x5c;
  }

  uint8_t inner[Hash::DIGEST_LEN];
  Hash h;
  h.begin();
  h.update(ipad, Hash::BLOCK_LEN);
  h.update(msg, msgLen);
  h.finish(inner);

  h.begin();
  h.update(opad, Hash::BLOCK_LEN);
  h.update(inner, Hash::DIGEST_LEN);
  h.finish(out);

  memset(keyBlock, 0, sizeof(keyBlock));
  memset(ipad, 0, sizeof(ipad));
  memset(opad, 0, sizeof(opad));
}

void hmacSha1(const uint8_t* key, size_t keyLen, const uint8_t* msg, size_t msgLen, uint8_t* out) {
  hmac<Sha1>(key, keyLen, msg, msgLen, out);
}

void hmacSha256(const uint8_t* key, size_t keyLen, const uint8_t* msg, size_t msgLen, uint8_t* out) {
  hmac<Sha256>(key, keyLen, msg, msgLen, out);
}

// --- PBKDF2-HMAC-SHA256 (RFC 8018) -------------------------------------------

void pbkdf2HmacSha256(const uint8_t* password, size_t passwordLen, const uint8_t* salt, size_t saltLen,
                      uint32_t iterations, uint8_t* out, size_t outLen) {
  uint32_t blockIndex = 1;
  // Salt || INT_32_BE(i), reused across blocks.
  uint8_t saltBlock[64 + 4];
  while (outLen > 0) {
    memcpy(saltBlock, salt, saltLen);
    storeBe32(saltBlock + saltLen, blockIndex);

    uint8_t u[Sha256::DIGEST_LEN];
    uint8_t t[Sha256::DIGEST_LEN];
    hmacSha256(password, passwordLen, saltBlock, saltLen + 4, u);
    memcpy(t, u, sizeof(t));
    for (uint32_t i = 1; i < iterations; i++) {
      hmacSha256(password, passwordLen, u, sizeof(u), u);
      for (size_t j = 0; j < sizeof(t); j++) t[j] ^= u[j];
    }

    size_t take = outLen < sizeof(t) ? outLen : sizeof(t);
    memcpy(out, t, take);
    out += take;
    outLen -= take;
    blockIndex++;
    memset(u, 0, sizeof(u));
    memset(t, 0, sizeof(t));
  }
}

bool constantTimeEquals(const uint8_t* a, const uint8_t* b, size_t len) {
  uint8_t diff = 0;
  for (size_t i = 0; i < len; i++) diff |= a[i] ^ b[i];
  return diff == 0;
}

}  // namespace inkkey
