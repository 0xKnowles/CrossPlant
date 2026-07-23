#pragma once

// Host stand-in for mbedtls AES-GCM with the same authenticate-then-decrypt
// contract: a wrong key or tampered ciphertext fails the tag check. The cipher
// itself is NOT AES (keystream = SHA-256 counter mode; tag = SHA-256 MAC) —
// it exists so vault round-trips and wrong-PIN paths behave realistically in
// the simulator. The device build links real mbedtls.

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "../../../../src/core/Sha.h"

#define MBEDTLS_GCM_ENCRYPT 1
#define MBEDTLS_GCM_DECRYPT 0
#define MBEDTLS_CIPHER_ID_AES 2

typedef struct {
  unsigned char key[32];
} mbedtls_gcm_context;

inline void mbedtls_gcm_init(mbedtls_gcm_context* ctx) { memset(ctx, 0, sizeof(*ctx)); }
inline void mbedtls_gcm_free(mbedtls_gcm_context* ctx) { memset(ctx, 0, sizeof(*ctx)); }

inline int mbedtls_gcm_setkey(mbedtls_gcm_context* ctx, int, const unsigned char* key, unsigned int keybits) {
  if (keybits != 256) return -1;
  memcpy(ctx->key, key, 32);
  return 0;
}

namespace inkkey_hostgcm {

inline void keystreamXor(const mbedtls_gcm_context* ctx, const unsigned char* iv, const unsigned char* in,
                         unsigned char* out, size_t len) {
  uint8_t block[32];
  uint32_t counter = 0;
  size_t o = 0;
  while (o < len) {
    inkkey::Sha256 h;
    h.begin();
    h.update(ctx->key, 32);
    h.update(iv, 12);
    uint8_t ctr[4] = {uint8_t(counter >> 24), uint8_t(counter >> 16), uint8_t(counter >> 8), uint8_t(counter)};
    h.update(ctr, 4);
    h.finish(block);
    for (size_t i = 0; i < 32 && o < len; i++, o++) out[o] = in[o] ^ block[i];
    counter++;
  }
}

inline void computeTag(const mbedtls_gcm_context* ctx, const unsigned char* iv, const unsigned char* ct, size_t len,
                       unsigned char* tag16) {
  uint8_t full[32];
  inkkey::Sha256 h;
  h.begin();
  h.update(ctx->key, 32);
  h.update(iv, 12);
  h.update(ct, len);
  h.finish(full);
  memcpy(tag16, full, 16);
}

}  // namespace inkkey_hostgcm

inline int mbedtls_gcm_crypt_and_tag(mbedtls_gcm_context* ctx, int mode, size_t length, const unsigned char* iv,
                                     size_t iv_len, const unsigned char*, size_t, const unsigned char* input,
                                     unsigned char* output, size_t tag_len, unsigned char* tag) {
  if (mode != MBEDTLS_GCM_ENCRYPT || iv_len != 12 || tag_len != 16) return -1;
  inkkey_hostgcm::keystreamXor(ctx, iv, input, output, length);
  inkkey_hostgcm::computeTag(ctx, iv, output, length, tag);
  return 0;
}

inline int mbedtls_gcm_auth_decrypt(mbedtls_gcm_context* ctx, size_t length, const unsigned char* iv, size_t iv_len,
                                    const unsigned char*, size_t, const unsigned char* tag, size_t tag_len,
                                    const unsigned char* input, unsigned char* output) {
  if (iv_len != 12 || tag_len != 16) return -1;
  unsigned char expected[16];
  inkkey_hostgcm::computeTag(ctx, iv, input, length, expected);
  if (!inkkey::constantTimeEquals(expected, tag, 16)) return -1;
  inkkey_hostgcm::keystreamXor(ctx, iv, input, output, length);
  return 0;
}
