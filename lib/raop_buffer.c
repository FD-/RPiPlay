/**
 *  Copyright (C) 2011-2012  Juho Vähä-Herttua
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
#include <math.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "raop_buffer.h"
#include "raop_rtp.h"

#include "crypto.h"
#include "compat.h"
#include "stream.h"

#define RAOP_BUFFER_LENGTH 32

typedef struct {
    /* Data available */
    int filled;

    /* RTP header */
    unsigned short seqnum;
    uint64_t timestamp;

    /* Payload data */
    unsigned int payload_size;
    void *payload_data;
} raop_buffer_entry_t;

struct raop_buffer_s {
    logger_t *logger;
    /* Key and IV used for decryption */
    unsigned char aeskey[RAOP_AESKEY_LEN];
    unsigned char aesiv[RAOP_AESIV_LEN];

    /* First and last seqnum */
    int is_empty;
    unsigned short first_seqnum;
    unsigned short last_seqnum;

    /* RTP buffer entries */
    raop_buffer_entry_t entries[RAOP_BUFFER_LENGTH];
};

void
raop_buffer_init_key_iv(raop_buffer_t *raop_buffer,
                        const unsigned char *aeskey,
                        const unsigned char *aesiv,
                        const unsigned char *ecdh_secret)
{

    // Initialization key
    unsigned char eaeskey[64];
    memcpy(eaeskey, aeskey, 16);

    sha_ctx_t *ctx = sha_init();
    sha_update(ctx, eaeskey, 16);
    sha_update(ctx, ecdh_secret, 32);
    sha_final(ctx, eaeskey, NULL);
    sha_destroy(ctx);

    memcpy(raop_buffer->aeskey, eaeskey, 16);
    memcpy(raop_buffer->aesiv, aesiv, RAOP_AESIV_LEN);

#ifdef DUMP_AUDIO
    if (file_keyiv != NULL) {
        fwrite(raop_buffer->aeskey, 16, 1, file_keyiv);
        fwrite(raop_buffer->aesiv, 16, 1, file_keyiv);
        fclose(file_keyiv);
    }
#endif
}

raop_buffer_t *
raop_buffer_init(logger_t *logger,
                 const unsigned char *aeskey,
                 const unsigned char *aesiv,
                 const unsigned char *ecdh_secret)
{
    raop_buffer_t *raop_buffer;
    assert(aeskey);
    assert(aesiv);
    assert(ecdh_secret);
    raop_buffer = calloc(1, sizeof(raop_buffer_t));
    if (!raop_buffer) {
        return NULL;
    }
    raop_buffer->logger = logger;
    raop_buffer_init_key_iv(raop_buffer, aeskey, aesiv, ecdh_secret);

    for (int i = 0; i < RAOP_BUFFER_LENGTH; i++) {
        raop_buffer_entry_t *entry = &raop_buffer->entries[i];
        entry->payload_data = NULL;
        entry->payload_size = 0;
    }

    raop_buffer->is_empty = 1;

    return raop_buffer;
}

void
raop_buffer_destroy(raop_buffer_t *raop_buffer)
{
    for (int i = 0; i < RAOP_BUFFER_LENGTH; i++) {
        raop_buffer_entry_t *entry = &raop_buffer->entries[i];
        if (entry->payload_data != NULL) {
            free(entry->payload_data);
        }
    }

    if (raop_buffer) {
        free(raop_buffer);
    }

#ifdef DUMP_AUDIO
    if (file_aac != NULL) {
        fclose(file_aac);
    }
    if (file_source != NULL) {
        fclose(file_source);
    }
#endif

}

static short
seqnum_cmp(unsigned short s1, unsigned short s2)
{
    return (s1 - s2);
}

//#define DUMP_AUDIO

#ifdef DUMP_AUDIO
static FILE* file_aac = NULL;
static FILE* file_source = NULL;
static FILE* file_keyiv = NULL;
#endif


int
raop_buffer_decrypt(raop_buffer_t *raop_buffer, unsigned char *data, unsigned char* output, unsigned int payload_size, unsigned int *outputlen)
{
    assert(raop_buffer);
    int encryptedlen;
#ifdef DUMP_AUDIO
    if (file_aac == NULL) {
        file_aac = fopen("/home/pi/Airplay.aac", "wb");
        file_source = fopen("/home/pi/Airplay.source", "wb");
        file_keyiv = fopen("/home/pi/Airplay.keyiv", "wb");
    }
    // Undecrypted file
    if (file_source != NULL) {
        fwrite(&data[12], payloadsize, 1, file_source);
    }
#endif

    encryptedlen = payload_size / 16*16;
    memset(output, 0, payload_size);
    // Need to be initialized internally
    aes_ctx_t *aes_ctx_audio = aes_cbc_init(raop_buffer->aeskey, raop_buffer->aesiv, AES_DECRYPT);
    aes_cbc_decrypt(aes_ctx_audio, &data[12], output, encryptedlen);
    aes_cbc_destroy(aes_ctx_audio);

    memcpy(output + encryptedlen, &data[12 + encryptedlen], payload_size - encryptedlen);
    *outputlen = payload_size;

#ifdef DUMP_AUDIO
    // Decrypted file
    if (file_aac != NULL) {
        fwrite(output, payloadsize, 1, file_aac);
    }
#endif

    return 1;
}

int
raop_buffer_enqueue(raop_buffer_t *raop_buffer, unsigned char *data, unsigned short datalen, uint64_t timestamp, int use_seqnum) {
    assert(raop_buffer);

    /* Check packet data length is valid */
    if (datalen < 12 || datalen > RAOP_PACKET_LEN) {
        return -1;
    }
    if (datalen == 16 && data[12] == 0x0 && data[13] == 0x68 && data[14] == 0x34 && data[15] == 0x0) {
        return 0;
    }
    int payload_size = datalen - 12;

    /* Get correct seqnum for the packet */
    unsigned short seqnum;
    if (use_seqnum) {
        seqnum = (data[2] << 8) | data[3];
    } else {
        seqnum = raop_buffer->first_seqnum;
    }

    /* If this packet is too late, just skip it */
    if (!raop_buffer->is_empty && seqnum_cmp(seqnum, raop_buffer->first_seqnum) < 0) {
        return 0;
    }

    /* Check that there is always space in the buffer, otherwise flush */
    if (seqnum_cmp(seqnum, raop_buffer->first_seqnum + RAOP_BUFFER_LENGTH) >= 0) {
        raop_buffer_flush(raop_buffer, seqnum);
    }

    /* Get entry corresponding our seqnum */
    raop_buffer_entry_t *entry = &raop_buffer->entries[seqnum % RAOP_BUFFER_LENGTH];
    if (entry->filled && seqnum_cmp(entry->seqnum, seqnum) == 0) {
        /* Packet resend, we can safely ignore */
        return 0;
    }

    /* Update the raop_buffer entry header */
    entry->seqnum = seqnum;
    entry->timestamp = timestamp;
    entry->filled = 1;

    entry->payload_data = malloc(payload_size);
    int decrypt_ret = raop_buffer_decrypt(raop_buffer, data, entry->payload_data, payload_size, &entry->payload_size);
    assert(decrypt_ret >= 0);
    assert(entry->payload_size <= payload_size);

    /* Update the raop_buffer seqnums */
    if (raop_buffer->is_empty) {
        raop_buffer->first_seqnum = seqnum;
        raop_buffer->last_seqnum = seqnum;
        raop_buffer->is_empty = 0;
    }
    if (seqnum_cmp(seqnum, raop_buffer->last_seqnum) > 0) {
        raop_buffer->last_seqnum = seqnum;
    }
    return 1;
}

void *
raop_buffer_dequeue(raop_buffer_t *raop_buffer, unsigned int *length, uint64_t *timestamp, int no_resend) {
    assert(raop_buffer);

    /* Calculate number of entries in the current buffer */
    short entry_count = seqnum_cmp(raop_buffer->last_seqnum, raop_buffer->first_seqnum)+1;

    /* Cannot dequeue from empty buffer */
    if (raop_buffer->is_empty || entry_count <= 0) {
        return NULL;
    }

    /* Get the first buffer entry for inspection */
    raop_buffer_entry_t *entry = &raop_buffer->entries[raop_buffer->first_seqnum % RAOP_BUFFER_LENGTH];
    if (no_resend) {
        /* If we do no resends, always return the first entry */
    } else if (!entry->filled) {
        /* Check how much we have space left in the buffer */
        if (entry_count < RAOP_BUFFER_LENGTH) {
            /* Return nothing and hope resend gets on time */
            return NULL;
        }
        /* Risk of buffer overrun, return empty buffer */
    }

    /* Update buffer and validate entry */
    raop_buffer->first_seqnum += 1;
    if (!entry->filled) {
        return NULL;
    }
    entry->filled = 0;

    /* Return entry payload buffer */
    *timestamp = entry->timestamp;
    *length = entry->payload_size;
    entry->payload_size = 0;
    void* data = entry->payload_data;
    entry->payload_data = NULL;
    return data;
}

void raop_buffer_handle_resends(raop_buffer_t *raop_buffer, raop_resend_cb_t resend_cb, void *opaque) {
    assert(raop_buffer);
    assert(resend_cb);

    if (seqnum_cmp(raop_buffer->first_seqnum, raop_buffer->last_seqnum) < 0) {
        int seqnum, count;

        for (seqnum = raop_buffer->first_seqnum; seqnum_cmp(seqnum, raop_buffer->last_seqnum) < 0; seqnum++) {
            raop_buffer_entry_t *entry = &raop_buffer->entries[seqnum % RAOP_BUFFER_LENGTH];
            if (entry->filled) {
                break;
            }
        }
        if (seqnum_cmp(seqnum, raop_buffer->first_seqnum) == 0) {
            return;
        }
        count = seqnum_cmp(seqnum, raop_buffer->first_seqnum);
        resend_cb(opaque, raop_buffer->first_seqnum, count);
    }
}

void raop_buffer_flush(raop_buffer_t *raop_buffer, int next_seq) {
    assert(raop_buffer);

    for (int i = 0; i < RAOP_BUFFER_LENGTH; i++) {
        if (raop_buffer->entries[i].payload_data) {
            free(raop_buffer->entries[i].payload_data);
            raop_buffer->entries[i].payload_size = 0;
        }
        raop_buffer->entries[i].filled = 0;
    }
    if (next_seq < 0 || next_seq > 0xffff) {
        raop_buffer->is_empty = 1;
    } else {
        raop_buffer->first_seqnum = next_seq;
        raop_buffer->last_seqnum = next_seq - 1;
    }
}
