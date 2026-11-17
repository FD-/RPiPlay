/**
 * RPiPlay - An open-source AirPlay mirroring server for Raspberry Pi
 * Copyright (C) 2019 Florian Draschbacher
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

/*
 * AAC renderer using fdk-aac for decoding and ALSA for rendering.
 *
 * Unlike the OpenMAX audio_render component (which can only output to the
 * firmware's "local" and "hdmi" destinations), this renderer plays through
 * the ALSA stack and therefore also reaches external I2S sound cards such
 * as the HiFiBerry DAC+ family.
*/

#include "audio_renderer.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#include <alsa/asoundlib.h>

#include "fdk-aac/libAACdec/include/aacdecoder_lib.h"

/* Maximum decoded frame size we are prepared to handle. AirPlay mirror
 * audio is AAC-ELD (480 samples/frame), but keep some headroom. */
#define DECODE_BUFFER_SAMPLES 2048
#define ALSA_CHANNELS 2
#define ALSA_SAMPLE_RATE 44100
/* PCM buffer sizes. Bigger buffers tolerate network jitter (and CPU load
 * spikes on the Pi Zero) without audible dropouts, at the cost of a
 * constant audio-video offset. */
#define ALSA_LATENCY_US 100000
#define ALSA_LATENCY_LOW_US 50000

#define ALSA_DEFAULT_DEVICE "default"
#define ALSA_HDMI_DEVICE "hdmi"

typedef struct audio_renderer_alsa_s {
    audio_renderer_t base;
    audio_renderer_config_t const *config;

    HANDLE_AACDECODER audio_decoder;
    INT_PCM *decode_buffer;

    snd_pcm_t *pcm;
    snd_mixer_t *mixer;
    snd_mixer_elem_t *mixer_elem;
    long mixer_db_min;
    long mixer_db_max;
} audio_renderer_alsa_t;

static const audio_renderer_funcs_t audio_renderer_alsa_funcs;

static void audio_renderer_alsa_destroy_decoder(audio_renderer_alsa_t *renderer) {
    if (renderer->audio_decoder) {
        aacDecoder_Close(renderer->audio_decoder);
        renderer->audio_decoder = NULL;
    }
}

static int audio_renderer_alsa_init_decoder(audio_renderer_alsa_t *renderer) {
    int ret = 0;
    renderer->audio_decoder = aacDecoder_Open(TT_MP4_RAW, 1);
    if (renderer->audio_decoder == NULL) {
        logger_log(renderer->base.logger, LOGGER_ERR, "aacDecoder open failed!");
        return -1;
    }
    /* ASC config binary data */
    UCHAR eld_conf[] = { 0xF8, 0xE8, 0x50, 0x00 };
    UCHAR *conf[] = { eld_conf };
    static UINT conf_len = sizeof(eld_conf);
    ret = aacDecoder_ConfigRaw(renderer->audio_decoder, conf, &conf_len);
    if (ret != AAC_DEC_OK) {
        logger_log(renderer->base.logger, LOGGER_ERR, "Unable to set configRaw");
        return -2;
    }
    /* The mirror stream is stereo, but make sure the decoder always
     * downmixes to exactly two output channels. */
    if (aacDecoder_SetParam(renderer->audio_decoder, AAC_PCM_MIN_OUTPUT_CHANNELS, ALSA_CHANNELS) != AAC_DEC_OK ||
        aacDecoder_SetParam(renderer->audio_decoder, AAC_PCM_MAX_OUTPUT_CHANNELS, ALSA_CHANNELS) != AAC_DEC_OK) {
        logger_log(renderer->base.logger, LOGGER_ERR, "Unable to set output channel count");
        return -2;
    }
    /* Interpolate lost or corrupt frames instead of dropping to silence. */
    if (aacDecoder_SetParam(renderer->audio_decoder, AAC_CONCEAL_METHOD, 1) != AAC_DEC_OK) {
        logger_log(renderer->base.logger, LOGGER_ERR, "Unable to enable error concealment");
        return -2;
    }

    CStreamInfo *aac_stream_info = aacDecoder_GetStreamInfo(renderer->audio_decoder);
    if (aac_stream_info == NULL) {
        logger_log(renderer->base.logger, LOGGER_ERR, "aacDecoder_GetStreamInfo failed!");
        return -3;
    }

    renderer->decode_buffer = malloc(DECODE_BUFFER_SAMPLES * ALSA_CHANNELS * sizeof(INT_PCM));
    if (renderer->decode_buffer == NULL) {
        return -4;
    }

    logger_log(renderer->base.logger, LOGGER_DEBUG, "> stream info: channel = %d\tsample_rate = %d\tframe_size = %d\taot = %d\tbitrate = %d",
            aac_stream_info->channelConfig, aac_stream_info->aacSampleRate,
            aac_stream_info->aacSamplesPerFrame, aac_stream_info->aot, aac_stream_info->bitRate);
    return 1;
}

static bool contains_case_insensitive(const char *haystack, const char *needle) {
    size_t needle_len = strlen(needle);
    if (needle_len == 0) return true;
    for (; *haystack; haystack++) {
        size_t i;
        for (i = 0; i < needle_len; i++) {
            if (tolower((unsigned char) haystack[i]) != tolower((unsigned char) needle[i])) break;
        }
        if (i == needle_len) return true;
    }
    return false;
}

/* Look for an ALSA card belonging to a HiFiBerry board and return its short
 * id (usable in a "sysdefault:CARD=..." device name). Returns true if found. */
static bool audio_renderer_alsa_find_hifiberry_card(char *card_id, size_t card_id_len) {
    int card = -1;
    while (snd_card_next(&card) >= 0 && card >= 0) {
        char ctl_name[16];
        snd_ctl_t *ctl = NULL;
        snd_ctl_card_info_t *info = NULL;
        bool found = false;

        snprintf(ctl_name, sizeof(ctl_name), "hw:%d", card);
        if (snd_ctl_open(&ctl, ctl_name, 0) < 0) continue;
        if (snd_ctl_card_info_malloc(&info) >= 0) {
            if (snd_ctl_card_info(ctl, info) >= 0) {
                const char *id = snd_ctl_card_info_get_id(info);
                const char *name = snd_ctl_card_info_get_name(info);
                if ((id && contains_case_insensitive(id, "hifiberry")) ||
                    (name && contains_case_insensitive(name, "hifiberry"))) {
                    if (id != NULL) {
                        snprintf(card_id, card_id_len, "%s", id);
                        found = true;
                    }
                }
            }
            snd_ctl_card_info_free(info);
        }
        snd_ctl_close(ctl);
        if (found) return true;
    }
    return false;
}

static int audio_renderer_alsa_open_pcm(audio_renderer_alsa_t *renderer, const char *device, unsigned int latency_us) {
    snd_pcm_t *pcm = NULL;
    int err;

    err = snd_pcm_open(&pcm, device, SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        logger_log(renderer->base.logger, LOGGER_ERR, "Could not open ALSA device %s: %s", device, snd_strerror(err));
        return -1;
    }

    err = snd_pcm_set_params(pcm, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED,
                             ALSA_CHANNELS, ALSA_SAMPLE_RATE, 1, latency_us);
    if (err < 0) {
        logger_log(renderer->base.logger, LOGGER_ERR, "Could not configure ALSA device %s: %s", device, snd_strerror(err));
        snd_pcm_close(pcm);
        return -1;
    }

    /* Start playback only once the whole buffer is primed. This gives a
     * constant latency and avoids audible underruns at start. */
    {
        snd_pcm_uframes_t buffer_size = 0, period_size = 0;
        snd_pcm_sw_params_t *sw_params = NULL;
        snd_pcm_get_params(pcm, &buffer_size, &period_size);
        snd_pcm_sw_params_alloca(&sw_params);
        snd_pcm_sw_params_current(pcm, sw_params);
        snd_pcm_sw_params_set_start_threshold(pcm, sw_params, buffer_size);
        snd_pcm_sw_params_set_avail_min(pcm, sw_params, period_size);
        snd_pcm_sw_params(pcm, sw_params);
    }

    renderer->pcm = pcm;
    return 0;
}

static void audio_renderer_alsa_init_mixer(audio_renderer_alsa_t *renderer) {
    int card = -1;
    snd_pcm_info_t *pcm_info = NULL;
    char ctl_name[16];
    snd_mixer_elem_t *elem = NULL;
    static const char *preferred[] = { "Master", "Digital", "Output", "PCM" };

    if (snd_pcm_info_malloc(&pcm_info) < 0) goto fail;
    if (snd_pcm_info(renderer->pcm, pcm_info) < 0) goto fail;
    card = snd_pcm_info_get_card(pcm_info);
    if (card < 0) goto fail;
    snd_pcm_info_free(pcm_info);
    pcm_info = NULL;

    snprintf(ctl_name, sizeof(ctl_name), "hw:%d", card);
    if (snd_mixer_open(&renderer->mixer, 0) < 0) {
        renderer->mixer = NULL;
        goto fail;
    }
    if (snd_mixer_attach(renderer->mixer, ctl_name) < 0 ||
        snd_mixer_selem_register(renderer->mixer, NULL, NULL) < 0 ||
        snd_mixer_load(renderer->mixer) < 0) {
        goto fail;
    }

    /* Prefer a sensibly named hardware mixer element, but accept any
     * element that has a playback volume. */
    for (size_t i = 0; i < sizeof(preferred)/sizeof(preferred[0]) && !renderer->mixer_elem; i++) {
        for (elem = snd_mixer_first_elem(renderer->mixer); elem; elem = snd_mixer_elem_next(elem)) {
            if (snd_mixer_selem_is_active(elem) && snd_mixer_selem_has_playback_volume(elem) &&
                snd_mixer_selem_get_name(elem) && strcmp(snd_mixer_selem_get_name(elem), preferred[i]) == 0) {
                renderer->mixer_elem = elem;
                break;
            }
        }
    }
    if (!renderer->mixer_elem) {
        for (elem = snd_mixer_first_elem(renderer->mixer); elem; elem = snd_mixer_elem_next(elem)) {
            if (snd_mixer_selem_is_active(elem) && snd_mixer_selem_has_playback_volume(elem)) {
                renderer->mixer_elem = elem;
                break;
            }
        }
    }
    if (!renderer->mixer_elem) goto fail;

    if (snd_mixer_selem_get_playback_dB_range(renderer->mixer_elem, &renderer->mixer_db_min, &renderer->mixer_db_max) < 0) {
        renderer->mixer_elem = NULL;
        goto fail;
    }
    snd_mixer_selem_set_playback_switch_all(renderer->mixer_elem, 1);
    logger_log(renderer->base.logger, LOGGER_INFO, "Using ALSA mixer control %s on hw:%d",
               snd_mixer_selem_get_name(renderer->mixer_elem), card);
    return;

fail:
    if (pcm_info) snd_pcm_info_free(pcm_info);
    if (renderer->mixer_elem == NULL && renderer->mixer) {
        snd_mixer_close(renderer->mixer);
        renderer->mixer = NULL;
    }
    logger_log(renderer->base.logger, LOGGER_WARNING, "No ALSA hardware volume control available");
}

static void audio_renderer_alsa_destroy_renderer(audio_renderer_alsa_t *renderer) {
    if (renderer->pcm) {
        snd_pcm_drop(renderer->pcm);
        snd_pcm_close(renderer->pcm);
        renderer->pcm = NULL;
    }
    if (renderer->mixer) {
        snd_mixer_close(renderer->mixer);
        renderer->mixer = NULL;
        renderer->mixer_elem = NULL;
    }
}

static int audio_renderer_alsa_init_renderer(audio_renderer_alsa_t *renderer) {
    unsigned int latency_us = renderer->config->low_latency ? ALSA_LATENCY_LOW_US : ALSA_LATENCY_US;
    char card_id[64];
    char dac_device[128];
    const char *device = NULL;

    if (renderer->config->alsa_device && renderer->config->alsa_device[0]) {
        device = renderer->config->alsa_device;
        if (audio_renderer_alsa_open_pcm(renderer, device, latency_us) != 0) {
            return -1;
        }
    } else if (renderer->config->device == AUDIO_DEVICE_HDMI) {
        device = ALSA_HDMI_DEVICE;
        if (audio_renderer_alsa_open_pcm(renderer, device, latency_us) != 0) {
            return -1;
        }
    } else {
        /* Analog output: prefer an external DAC such as a HiFiBerry board,
         * and fall back to the system default device otherwise. */
        if (audio_renderer_alsa_find_hifiberry_card(card_id, sizeof(card_id))) {
            snprintf(dac_device, sizeof(dac_device), "sysdefault:CARD=%s", card_id);
            if (audio_renderer_alsa_open_pcm(renderer, dac_device, latency_us) == 0) {
                device = dac_device;
            } else {
                logger_log(renderer->base.logger, LOGGER_WARNING, "Could not open HiFiBerry device, falling back to %s", ALSA_DEFAULT_DEVICE);
            }
        }
        if (device == NULL) {
            device = ALSA_DEFAULT_DEVICE;
            if (audio_renderer_alsa_open_pcm(renderer, device, latency_us) != 0) {
                return -1;
            }
        }
    }

    logger_log(renderer->base.logger, LOGGER_INFO, "Playing audio on ALSA device %s", device);

    audio_renderer_alsa_init_mixer(renderer);
    return 1;
}

audio_renderer_t *audio_renderer_alsa_init(logger_t *logger, video_renderer_t *video_renderer, audio_renderer_config_t const *config) {
    audio_renderer_alsa_t *renderer;
    (void) video_renderer; // The alsa renderer does not share resources with the video renderer

    renderer = calloc(1, sizeof(audio_renderer_alsa_t));
    if (!renderer) {
        return NULL;
    }
    renderer->base.logger = logger;
    renderer->base.funcs = &audio_renderer_alsa_funcs;
    renderer->base.type = AUDIO_RENDERER_ALSA;
    renderer->config = config;

    if (audio_renderer_alsa_init_decoder(renderer) != 1) {
        free(renderer->decode_buffer);
        audio_renderer_alsa_destroy_decoder(renderer);
        free(renderer);
        return NULL;
    }

    if (audio_renderer_alsa_init_renderer(renderer) != 1) {
        audio_renderer_alsa_destroy_renderer(renderer);
        free(renderer->decode_buffer);
        audio_renderer_alsa_destroy_decoder(renderer);
        free(renderer);
        return NULL;
    }

    return &renderer->base;
}

static void audio_renderer_alsa_start(audio_renderer_t *renderer) {
    audio_renderer_alsa_t *r = (audio_renderer_alsa_t *)renderer;
    snd_pcm_prepare(r->pcm);
}

static void audio_renderer_alsa_render_buffer(audio_renderer_t *renderer, raop_ntp_t *ntp, unsigned char *data, int data_len, uint64_t pts) {
    (void) ntp;
    (void) pts; // The alsa renderer plays as soon as data arrives

    if (data_len == 0) return;

    audio_renderer_alsa_t *r = (audio_renderer_alsa_t *)renderer;

    UCHAR *in_buffer[1] = { data };
    UINT in_buffer_size = data_len;
    UINT in_valid = data_len;
    AAC_DECODER_ERROR error = aacDecoder_Fill(r->audio_decoder, in_buffer, &in_buffer_size, &in_valid);
    if (error != AAC_DEC_OK) {
        logger_log(renderer->logger, LOGGER_ERR, "aacDecoder_Fill error : %x", error);
        return;
    }

    error = aacDecoder_DecodeFrame(r->audio_decoder, r->decode_buffer, DECODE_BUFFER_SAMPLES * ALSA_CHANNELS, 0);
    if (error != AAC_DEC_OK) {
        /* Feed the decoder a concealed frame so playback stays smooth. */
        error = aacDecoder_DecodeFrame(r->audio_decoder, r->decode_buffer, DECODE_BUFFER_SAMPLES * ALSA_CHANNELS, AACDEC_CONCEAL);
        if (error != AAC_DEC_OK) {
            logger_log(renderer->logger, LOGGER_ERR, "aacDecoder_DecodeFrame error : 0x%x", error);
            return;
        }
        logger_log(renderer->logger, LOGGER_DEBUG, "Concealed audio frame");
    }

    CStreamInfo *stream_info = aacDecoder_GetStreamInfo(r->audio_decoder);
    if (stream_info == NULL || stream_info->frameSize <= 0) {
        return;
    }

    snd_pcm_sframes_t frames = stream_info->frameSize;
    INT_PCM *out = r->decode_buffer;
    while (frames > 0) {
        snd_pcm_sframes_t written = snd_pcm_writei(r->pcm, out, frames);
        if (written > 0) {
            out += written * ALSA_CHANNELS;
            frames -= written;
        } else if (written == -EPIPE || written == -ESTRPIPE) {
            if (snd_pcm_recover(r->pcm, written, 1) < 0) {
                logger_log(renderer->logger, LOGGER_ERR, "Audio output underrun and recovery failed");
                break;
            }
        } else {
            logger_log(renderer->logger, LOGGER_ERR, "snd_pcm_writei failed: %s", snd_strerror(written));
            break;
        }
    }
}

static void audio_renderer_alsa_set_volume(audio_renderer_t *renderer, float volume) {
    audio_renderer_alsa_t *r = (audio_renderer_alsa_t *)renderer;
    if (!r->mixer_elem) return;

    /* The AirPlay volume arrives in dB, ranging from 0 to -144 (mute). */
    if (volume <= -144.0f) {
        snd_mixer_selem_set_playback_switch_all(r->mixer_elem, 0);
        return;
    }
    snd_mixer_selem_set_playback_switch_all(r->mixer_elem, 1);

    long db = (long) volume;
    if (db > r->mixer_db_max) db = r->mixer_db_max;
    if (db < r->mixer_db_min) db = r->mixer_db_min;
    if (snd_mixer_selem_set_playback_dB_all(r->mixer_elem, db, 0) < 0) {
        logger_log(renderer->logger, LOGGER_DEBUG, "Could not set ALSA volume");
    }
}

static void audio_renderer_alsa_flush(audio_renderer_t *renderer) {
    audio_renderer_alsa_t *r = (audio_renderer_alsa_t *)renderer;
    snd_pcm_drop(r->pcm);
    snd_pcm_prepare(r->pcm);
}

static void audio_renderer_alsa_destroy(audio_renderer_t *renderer) {
    if (renderer) {
        audio_renderer_alsa_t *r = (audio_renderer_alsa_t *)renderer;
        audio_renderer_alsa_destroy_renderer(r);
        audio_renderer_alsa_destroy_decoder(r);
        free(r->decode_buffer);
        free(renderer);
    }
}

static const audio_renderer_funcs_t audio_renderer_alsa_funcs = {
    .start = audio_renderer_alsa_start,
    .render_buffer = audio_renderer_alsa_render_buffer,
    .set_volume = audio_renderer_alsa_set_volume,
    .flush = audio_renderer_alsa_flush,
    .destroy = audio_renderer_alsa_destroy,
};
