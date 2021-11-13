/**
 *  Copyright (C) 2018  Juho Vähä-Herttua
 *  Copyright (C) 2020  Jaslo Ziska
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <openssl/sha.h> // for SHA512_DIGEST_LENGTH

#include "pairing.h"
#include "crypto.h"

#define SALT_KEY "Pair-Verify-AES-Key"
#define SALT_IV "Pair-Verify-AES-IV"

struct pairing_s {
    ed25519_key_t *ed;
};

typedef enum {
    STATUS_INITIAL,
    STATUS_SETUP,
    STATUS_HANDSHAKE,
    STATUS_FINISHED
} status_t;

struct pairing_session_s {
    status_t status;

    ed25519_key_t *ed_ours;
    ed25519_key_t *ed_theirs;

    x25519_key_t *ecdh_ours;
    x25519_key_t *ecdh_theirs;
    unsigned char ecdh_secret[X25519_KEY_SIZE];
};

static int
derive_key_internal(pairing_session_t *session, const unsigned char *salt, unsigned int saltlen, unsigned char *key, unsigned int keylen)
{
    unsigned char hash[SHA512_DIGEST_LENGTH];

    if (keylen > sizeof(hash)) {
        return -1;
    }

    sha_ctx_t *ctx = sha_init();
    sha_update(ctx, salt, saltlen);
    sha_update(ctx, session->ecdh_secret, X25519_KEY_SIZE);
    sha_final(ctx, hash, NULL);
    sha_destroy(ctx);

    memcpy(key, hash, keylen);
    return 0;
}

pairing_t *
pairing_init_generate()
{
    pairing_t *pairing;

    pairing = calloc(1, sizeof(pairing_t));
    if (!pairing) {
        return NULL;
    }

    pairing->ed = ed25519_key_generate();

    return pairing;
}

void
pairing_get_public_key(pairing_t *pairing, unsigned char public_key[ED25519_KEY_SIZE])
{
    assert(pairing);
    ed25519_key_get_raw(public_key, pairing->ed);
}

void
pairing_get_ecdh_secret_key(pairing_session_t *session, unsigned char ecdh_secret[X25519_KEY_SIZE])
{
    assert(session);
    memcpy(ecdh_secret, session->ecdh_secret, X25519_KEY_SIZE);
}


pairing_session_t *
pairing_session_init(pairing_t *pairing)
{
    pairing_session_t *session;

    if (!pairing) {
        return NULL;
    }

    session = calloc(1, sizeof(pairing_session_t));
    if (!session) {
        return NULL;
    }

    session->ed_ours = ed25519_key_copy(pairing->ed);

    session->status = STATUS_INITIAL;

    return session;
}

void
pairing_session_set_setup_status(pairing_session_t *session)
{
    assert(session);
    session->status = STATUS_SETUP;
}

int
pairing_session_check_handshake_status(pairing_session_t *session)
{
    assert(session);
    if (session->status != STATUS_SETUP) {
        return -1;
    }
    return 0;
}

int
pairing_session_handshake(pairing_session_t *session, const unsigned char ecdh_key[X25519_KEY_SIZE],
                          const unsigned char ed_key[ED25519_KEY_SIZE])
{
    assert(session);

    if (session->status == STATUS_FINISHED) {
        return -1;
    }

    session->ecdh_theirs = x25519_key_from_raw(ecdh_key);
    session->ed_theirs = ed25519_key_from_raw(ed_key);

    session->ecdh_ours = x25519_key_generate();

    x25519_derive_secret(session->ecdh_secret, session->ecdh_ours, session->ecdh_theirs);

    session->status = STATUS_HANDSHAKE;
    return 0;
}

int
pairing_session_get_public_key(pairing_session_t *session, unsigned char ecdh_key[X25519_KEY_SIZE])
{
    assert(session);

    if (session->status != STATUS_HANDSHAKE) {
        return -1;
    }

    x25519_key_get_raw(ecdh_key, session->ecdh_ours);

    return 0;
}

int
pairing_session_get_signature(pairing_session_t *session, unsigned char signature[PAIRING_SIG_SIZE])
{
    unsigned char sig_msg[PAIRING_SIG_SIZE];
    unsigned char key[AES_128_BLOCK_SIZE];
    unsigned char iv[AES_128_BLOCK_SIZE];
    aes_ctx_t *aes_ctx;

    assert(session);

    if (session->status != STATUS_HANDSHAKE) {
        return -1;
    }

    /* First sign the public ECDH keys of both parties */
    x25519_key_get_raw(sig_msg, session->ecdh_ours);
    x25519_key_get_raw(sig_msg + X25519_KEY_SIZE, session->ecdh_theirs);

    ed25519_sign(signature, PAIRING_SIG_SIZE, sig_msg, PAIRING_SIG_SIZE, session->ed_ours);

    /* Then encrypt the result with keys derived from the shared secret */
    derive_key_internal(session, (const unsigned char *) SALT_KEY, strlen(SALT_KEY), key, sizeof(key));
    derive_key_internal(session, (const unsigned char *) SALT_IV, strlen(SALT_IV), iv, sizeof(iv));

    aes_ctx = aes_ctr_init(key, iv);
    aes_ctr_encrypt(aes_ctx, signature, signature, PAIRING_SIG_SIZE);
    aes_ctr_destroy(aes_ctx);

    return 0;
}

int
pairing_session_finish(pairing_session_t *session, const unsigned char signature[PAIRING_SIG_SIZE])
{
    unsigned char sig_buffer[PAIRING_SIG_SIZE];
    unsigned char sig_msg[PAIRING_SIG_SIZE];
    unsigned char key[AES_128_BLOCK_SIZE];
    unsigned char iv[AES_128_BLOCK_SIZE];
    aes_ctx_t *aes_ctx;

    assert(session);

    if (session->status != STATUS_HANDSHAKE) {
        return -1;
    }

    /* First decrypt the signature with keys derived from the shared secret */
    derive_key_internal(session, (const unsigned char *) SALT_KEY, strlen(SALT_KEY), key, sizeof(key));
    derive_key_internal(session, (const unsigned char *) SALT_IV, strlen(SALT_IV), iv, sizeof(iv));

    aes_ctx = aes_ctr_init(key, iv);
    /* One fake round for the initial handshake encryption */
    aes_ctr_encrypt(aes_ctx, sig_buffer, sig_buffer, PAIRING_SIG_SIZE);
    aes_ctr_encrypt(aes_ctx, signature, sig_buffer, PAIRING_SIG_SIZE);
    aes_ctr_destroy(aes_ctx);

    /* Then verify the signature with public ECDH keys of both parties */
    x25519_key_get_raw(sig_msg, session->ecdh_theirs);
    x25519_key_get_raw(sig_msg + X25519_KEY_SIZE, session->ecdh_ours);

    if (!ed25519_verify(sig_buffer, PAIRING_SIG_SIZE, sig_msg, PAIRING_SIG_SIZE, session->ed_theirs)) {
        return -2;
    }

    session->status = STATUS_FINISHED;
    return 0;
}

void
pairing_session_destroy(pairing_session_t *session)
{
    if (session) {
        ed25519_key_destroy(session->ed_ours);
        ed25519_key_destroy(session->ed_theirs);

        x25519_key_destroy(session->ecdh_ours);
        x25519_key_destroy(session->ecdh_theirs);

        free(session);
    }
}

void
pairing_destroy(pairing_t *pairing)
{
    if (pairing) {
        ed25519_key_destroy(pairing->ed);
        free(pairing);
    }
}
