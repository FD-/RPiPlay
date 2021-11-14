/**
 * RPiPlay - An open-source AirPlay mirroring server for Raspberry Pi
 * Copyright (C) 2019 Florian Draschbacher
 * Copyright (C) 2020 Jaslo Ziska
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include "crypto.h"

#include <openssl/evp.h>
#include <openssl/err.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

struct aes_ctx_s {
    EVP_CIPHER_CTX *cipher_ctx;
    uint8_t key[AES_128_BLOCK_SIZE];
    uint8_t iv[AES_128_BLOCK_SIZE];
    aes_direction_t direction;
    uint8_t block_offset;
};

uint8_t waste[AES_128_BLOCK_SIZE];

// Common AES utilities

void handle_error(const char* location) {
    long error = ERR_get_error();
    const char* error_str = ERR_error_string(error, NULL);
    fprintf(stderr, "Crypto error at %s: %s\n", location, error_str);
    exit(EXIT_FAILURE);
}

aes_ctx_t *aes_init(const uint8_t *key, const uint8_t *iv, const EVP_CIPHER *type, aes_direction_t direction) {
    aes_ctx_t *ctx = malloc(sizeof(aes_ctx_t));
    assert(ctx != NULL);
    ctx->cipher_ctx = EVP_CIPHER_CTX_new();
    assert(ctx->cipher_ctx != NULL);

    ctx->block_offset = 0;
    ctx->direction = direction;

    if (direction == AES_ENCRYPT) {
        if (!EVP_EncryptInit_ex(ctx->cipher_ctx, type, NULL, key, iv)) {
            handle_error(__func__);
        }
    } else {
        if (!EVP_DecryptInit_ex(ctx->cipher_ctx, type, NULL, key, iv)) {
            handle_error(__func__);
        }
    }

    memcpy(ctx->key, key, AES_128_BLOCK_SIZE);
    memcpy(ctx->iv, iv, AES_128_BLOCK_SIZE);
    EVP_CIPHER_CTX_set_padding(ctx->cipher_ctx, 0);
    return ctx;
}

void aes_encrypt(aes_ctx_t *ctx, const uint8_t *in, uint8_t *out, int in_len) {
    int out_len_e = 0;
    if (!EVP_EncryptUpdate(ctx->cipher_ctx, out, &out_len_e, in, in_len)) {
        handle_error(__func__);
    }
    int out_len_f = in_len - out_len_e;
    if (!EVP_EncryptFinal_ex(ctx->cipher_ctx, out + out_len_e, &out_len_f)) {
        handle_error(__func__);
    }
    assert(out_len_e + out_len_f <= in_len);
}

void aes_decrypt(aes_ctx_t *ctx, const uint8_t *in, uint8_t *out, int in_len) {
    int out_len_d = 0;
    if (!EVP_DecryptUpdate(ctx->cipher_ctx, out, &out_len_d, in, in_len)) {
        handle_error(__func__);
    }
    int out_len_f = in_len - out_len_d;
    if (!EVP_DecryptFinal_ex(ctx->cipher_ctx, out + out_len_d, &out_len_f)) {
        handle_error(__func__);
    }
    assert(out_len_f + out_len_d <= in_len);
}

void aes_destroy(aes_ctx_t *ctx) {
    if (ctx) {
        EVP_CIPHER_CTX_free(ctx->cipher_ctx);
        free(ctx);
    }
}

void aes_reset(aes_ctx_t *ctx, const EVP_CIPHER *type, aes_direction_t direction) {
    if (!EVP_CIPHER_CTX_reset(ctx->cipher_ctx)) {
        handle_error(__func__);
    }

    if (direction == AES_ENCRYPT) {
        if (!EVP_EncryptInit_ex(ctx->cipher_ctx, type, NULL, ctx->key, ctx->iv)) {
            handle_error(__func__);
        }
    } else {
        if (!EVP_DecryptInit_ex(ctx->cipher_ctx, type, NULL, ctx->key, ctx->iv)) {
            handle_error(__func__);
        }
    }
}

// AES CTR

aes_ctx_t *aes_ctr_init(const uint8_t *key, const uint8_t *iv) {
    return aes_init(key, iv, EVP_aes_128_ctr(), AES_ENCRYPT);
}

void aes_ctr_encrypt(aes_ctx_t *ctx, const uint8_t *in, uint8_t *out, int len) {
    aes_encrypt(ctx, in, out, len);
    ctx->block_offset = (ctx->block_offset + len) % AES_128_BLOCK_SIZE;
}

void aes_ctr_start_fresh_block(aes_ctx_t *ctx) {
    // Is there a better way to do this?
    if (ctx->block_offset == 0) return;
    aes_ctr_encrypt(ctx, waste, waste, AES_128_BLOCK_SIZE - ctx->block_offset);
}

void aes_ctr_decrypt(aes_ctx_t *ctx, const uint8_t *in, uint8_t *out, int len) {
    aes_encrypt(ctx, in, out, len);
}

void aes_ctr_reset(aes_ctx_t *ctx) {
    aes_reset(ctx, EVP_aes_128_ctr(), AES_ENCRYPT);
}

void aes_ctr_destroy(aes_ctx_t *ctx) {
    aes_destroy(ctx);
}

// AES CBC

aes_ctx_t *aes_cbc_init(const uint8_t *key, const uint8_t *iv, aes_direction_t direction) {
    return aes_init(key, iv, EVP_aes_128_cbc(), direction);
}

void aes_cbc_encrypt(aes_ctx_t *ctx, const uint8_t *in, uint8_t *out, int len) {
    assert(ctx->direction == AES_ENCRYPT);
    aes_encrypt(ctx, in, out, len);
}

void aes_cbc_decrypt(aes_ctx_t *ctx, const uint8_t *in, uint8_t *out, int len) {
    assert(ctx->direction == AES_DECRYPT);
    aes_decrypt(ctx, in, out, len);
}

void aes_cbc_reset(aes_ctx_t *ctx) {
    aes_reset(ctx, EVP_aes_128_ctr(), ctx->direction);
}

void aes_cbc_destroy(aes_ctx_t *ctx) {
    aes_destroy(ctx);
}

// X25519

struct x25519_key_s {
    EVP_PKEY *pkey;
};

x25519_key_t *x25519_key_generate(void) {
    x25519_key_t *key;
    EVP_PKEY_CTX *pctx;

    key = calloc(1, sizeof(x25519_key_t));
    assert(key);

    pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_X25519, NULL);
    if (!pctx) {
        handle_error(__func__);
    }
    if (!EVP_PKEY_keygen_init(pctx)) {
        handle_error(__func__);
    }
    if (!EVP_PKEY_keygen(pctx, &key->pkey)) {
        handle_error(__func__);
    }
    EVP_PKEY_CTX_free(pctx);

    return key;
}

x25519_key_t *x25519_key_from_raw(const unsigned char data[X25519_KEY_SIZE]) {
    x25519_key_t *key;

    key = malloc(sizeof(x25519_key_t));
    assert(key);

    key->pkey = EVP_PKEY_new_raw_public_key(EVP_PKEY_X25519, NULL, data, X25519_KEY_SIZE);
    if (!key->pkey) {
        handle_error(__func__);
    }

    return key;
}

void x25519_key_get_raw(unsigned char data[X25519_KEY_SIZE], const x25519_key_t *key) {
    assert(key);
    if (!EVP_PKEY_get_raw_public_key(key->pkey, data, &(size_t) {X25519_KEY_SIZE})) {
        handle_error(__func__);
    }
}

void x25519_key_destroy(x25519_key_t *key) {
    if (key) {
        EVP_PKEY_free(key->pkey);
        free(key);
    }
}

void x25519_derive_secret(unsigned char secret[X25519_KEY_SIZE], const x25519_key_t *ours, const x25519_key_t *theirs) {
    EVP_PKEY_CTX *pctx;

    assert(ours);
    assert(theirs);

    pctx = EVP_PKEY_CTX_new(ours->pkey, NULL);
    if (!pctx) {
        handle_error(__func__);
    }
    if (!EVP_PKEY_derive_init(pctx)) {
        handle_error(__func__);
    }
    if (!EVP_PKEY_derive_set_peer(pctx, theirs->pkey)) {
        handle_error(__func__);
    }
    if (!EVP_PKEY_derive(pctx, secret, &(size_t) {X25519_KEY_SIZE})) {
        handle_error(__func__);
    }
    EVP_PKEY_CTX_free(pctx);
}

// ED25519

struct ed25519_key_s {
    EVP_PKEY *pkey;
};

ed25519_key_t *ed25519_key_generate(void) {
    ed25519_key_t *key;
    EVP_PKEY_CTX *pctx;

    key = calloc(1, sizeof(ed25519_key_t));
    assert(key);

    pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, NULL);
    if (!pctx) {
        handle_error(__func__);
    }
    if (!EVP_PKEY_keygen_init(pctx)) {
        handle_error(__func__);
    }
    if (!EVP_PKEY_keygen(pctx, &key->pkey)) {
        handle_error(__func__);
    }
    EVP_PKEY_CTX_free(pctx);

    return key;
}

ed25519_key_t *ed25519_key_from_raw(const unsigned char data[ED25519_KEY_SIZE]) {
    ed25519_key_t *key;

    key = malloc(sizeof(ed25519_key_t));
    assert(key);

    key->pkey = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, NULL, data, ED25519_KEY_SIZE);
    if (!key->pkey) {
        handle_error(__func__);
    }

    return key;
}

void ed25519_key_get_raw(unsigned char data[ED25519_KEY_SIZE], const ed25519_key_t *key) {
    assert(key);
    if (!EVP_PKEY_get_raw_public_key(key->pkey, data, &(size_t) {ED25519_KEY_SIZE})) {
        handle_error(__func__);
    }
}

ed25519_key_t *ed25519_key_copy(const ed25519_key_t *key) {
    ed25519_key_t *new_key;

    assert(key);

    new_key = malloc(sizeof(ed25519_key_t));
    assert(new_key);

    new_key->pkey = key->pkey;
    if (!EVP_PKEY_up_ref(key->pkey)) {
        handle_error(__func__);
    }

    return new_key;
}

void ed25519_sign(unsigned char *signature, size_t signature_len,
                  const unsigned char *data, size_t data_len,
                  const ed25519_key_t *key)
{
    EVP_MD_CTX *mctx;

    mctx = EVP_MD_CTX_new();
    if (!mctx) {
        handle_error(__func__);
    }

    if (!EVP_DigestSignInit(mctx, NULL, NULL, NULL, key->pkey)) {
        handle_error(__func__);
    }
    if (!EVP_DigestSign(mctx, signature, &signature_len, data, data_len)) {
        handle_error(__func__);
    }

    EVP_MD_CTX_free(mctx);
}

int ed25519_verify(const unsigned char *signature, size_t signature_len,
                   const unsigned char *data, size_t data_len,
                   const ed25519_key_t *key)
{
    EVP_MD_CTX *mctx;

    mctx = EVP_MD_CTX_new();
    if (!mctx) {
        handle_error(__func__);
    }

    if (!EVP_DigestVerifyInit(mctx, NULL, NULL, NULL, key->pkey)) {
        handle_error(__func__);
    }

    int ret = EVP_DigestVerify(mctx, signature, signature_len, data, data_len);
    if (ret < 0) {
        handle_error(__func__);
    }

    EVP_MD_CTX_free(mctx);

    return ret;
}

void ed25519_key_destroy(ed25519_key_t *key) {
    if (key) {
        EVP_PKEY_free(key->pkey);
        free(key);
    }
}

// SHA 512

struct sha_ctx_s {
    EVP_MD_CTX *digest_ctx;
};

sha_ctx_t *sha_init() {
    sha_ctx_t *ctx = malloc(sizeof(sha_ctx_t));
    assert(ctx != NULL);
    ctx->digest_ctx = EVP_MD_CTX_new();
    assert(ctx->digest_ctx != NULL);

    if (!EVP_DigestInit_ex(ctx->digest_ctx, EVP_sha512(), NULL)) {
        handle_error(__func__);
    }
    return ctx;
}

void sha_update(sha_ctx_t *ctx, const uint8_t *in, int len) {
    if (!EVP_DigestUpdate(ctx->digest_ctx, in, len)) {
        handle_error(__func__);
    }
}

void sha_final(sha_ctx_t *ctx, uint8_t *out, unsigned int *len) {
    if (!EVP_DigestFinal_ex(ctx->digest_ctx, out, len)) {
        handle_error(__func__);
    }
}

void sha_reset(sha_ctx_t *ctx) {
    if (!EVP_MD_CTX_reset(ctx->digest_ctx) ||
        !EVP_DigestInit_ex(ctx->digest_ctx, EVP_sha512(), NULL)) {

        handle_error(__func__);
    }
}

void sha_destroy(sha_ctx_t *ctx) {
    if (ctx) {
        EVP_MD_CTX_free(ctx->digest_ctx);
        free(ctx);
    }
}
