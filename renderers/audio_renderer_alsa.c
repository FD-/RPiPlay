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
 * AAC renderer using fdk-aac for decoding and ALSA for rendering
*/

#include "audio_renderer.h"

#include <stdlib.h>

#include "fdk-aac/libAACdec/include/aacdecoder_lib.h"
#include "alsa/asoundlib.h"

// Check ALSA error code and show log
#define CHK_ALSA_ERRNO(err, msg, r) \
     if ( (err) < 0 )\
        logger_log(r, LOGGER_ERR, "%s: %s", msg, snd_strerror(err))

static bool mHasCtrlVolume;


typedef struct audio_renderer_alsa_s {
    audio_renderer_t base;
    audio_renderer_config_t const *config;

    HANDLE_AACDECODER audio_decoder;
    snd_pcm_t *audio_renderer;

    // For volume control
    snd_ctl_t *ctl;
    snd_ctl_elem_id_t *ctl_elem_id;

} audio_renderer_alsa_t;

static const audio_renderer_funcs_t audio_renderer_alsa_funcs;

static void audio_renderer_alsa_destroy_decoder(audio_renderer_alsa_t *renderer) {
    aacDecoder_Close(renderer->audio_decoder);
}

static int audio_renderer_alsa_init_decoder(audio_renderer_alsa_t *renderer) {
    int ret;
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
    CStreamInfo *aac_stream_info = aacDecoder_GetStreamInfo(renderer->audio_decoder);
    if (aac_stream_info == NULL) {
        logger_log(renderer->base.logger, LOGGER_ERR, "aacDecoder_GetStreamInfo failed!");
        return -3;
    }

    logger_log(renderer->base.logger, LOGGER_DEBUG, "> stream info: channel = %d\tsample_rate = %d\tframe_size = %d\taot = %d\tbitrate = %d",   \
            aac_stream_info->channelConfig, aac_stream_info->aacSampleRate,
            aac_stream_info->aacSamplesPerFrame, aac_stream_info->aot, aac_stream_info->bitRate);
    return 1;
}

static void audio_renderer_rpi_destroy_renderer(audio_renderer_alsa_t *renderer) {
    CHK_ALSA_ERRNO( snd_pcm_drain( renderer->audio_renderer ), "ALSA PCM drain", renderer->base.logger);
    CHK_ALSA_ERRNO( snd_pcm_close( renderer->audio_renderer ), "ALSA PCM close", renderer->base.logger);
    CHK_ALSA_ERRNO( snd_ctl_close(renderer->ctl), "ALSA CTL close", renderer->base.logger);
}

static int audio_renderer_rpi_init_renderer( audio_renderer_alsa_t *renderer,
                                             __attribute__((unused)) video_renderer_t *video_renderer )
{
    const char *device;
    if ( renderer->config->alsaString )
        device = renderer->config->alsaString;
    else
        device = "default";
    // Opening a connection to PCM for audio playback
    int err = snd_pcm_open(&renderer->audio_renderer, device, SND_PCM_STREAM_PLAYBACK, 0);
    if ( err < 0 ) {
        logger_log(renderer->base.logger, LOGGER_ERR, snd_strerror(err));
        return -51;
    }

    err = snd_pcm_set_params(renderer->audio_renderer,
                             SND_PCM_FORMAT_S16_LE,
                             SND_PCM_ACCESS_RW_INTERLEAVED,
                             2,
                             44100,
                             1,
                             100000);

    if ( err < 0 ) {
        logger_log(renderer->base.logger, LOGGER_ERR, snd_strerror(err));
        return -52;
    }
    // Opening a connection to the sound card to adjust the volume
    CHK_ALSA_ERRNO( snd_ctl_open(&renderer->ctl, device, 0), "Ctrl open", renderer->base.logger );
    CHK_ALSA_ERRNO( snd_ctl_elem_id_malloc(&renderer->ctl_elem_id), "Ctrl mem allocation", renderer->base.logger );
    snd_ctl_elem_id_set_interface(renderer->ctl_elem_id, SND_CTL_ELEM_IFACE_MIXER);

    // Finding the right sound card item to adjust the volume
    snd_ctl_elem_list_t* list;
    snd_ctl_elem_list_alloca(&list);
    CHK_ALSA_ERRNO( snd_ctl_elem_list(renderer->ctl, list), "Get ALSA control element list", renderer->base.logger );
    uint count = snd_ctl_elem_list_get_count(list);
    CHK_ALSA_ERRNO( snd_ctl_elem_list_alloc_space(list, count), "Alloc space for ALSA ctrl ID", renderer->base.logger );
    CHK_ALSA_ERRNO( snd_ctl_elem_list(renderer->ctl, list), "Get ALSA control element list", renderer->base.logger );

    mHasCtrlVolume = true;
    for ( ;count != 0; count-- ) {
        if ( strstr( snd_ctl_elem_list_get_name(list, count - 1), "Playback Volume" ) ) {
            snd_ctl_elem_id_set_numid( renderer->ctl_elem_id,
                                       snd_ctl_elem_list_get_numid(list, count - 1) );
            break;
        }

        if (count == 1) {
            logger_log(renderer->base.logger, LOGGER_INFO, "Cannot control Volume for selected device");
            mHasCtrlVolume = false;
        }
    }

    snd_ctl_elem_list_free_space(list);

    return 1;
}

audio_renderer_t *audio_renderer_alsa_init(logger_t *logger, video_renderer_t *video_renderer, audio_renderer_config_t const *config) {
    audio_renderer_alsa_t *renderer;
    renderer = calloc(1, sizeof(audio_renderer_alsa_t));
    if (!renderer) {
        return NULL;
    }
    renderer->base.logger = logger;
    renderer->base.funcs = &audio_renderer_alsa_funcs;
    renderer->base.type = AUDIO_RENDERER_RPI;
    // Only refer to an existing video renderer if it's an RPI renderer,
    // in which case we have to share resources
    if (video_renderer && video_renderer->type != VIDEO_RENDERER_RPI) {
        video_renderer = NULL;
    }
    renderer->config = config;

    if (audio_renderer_alsa_init_decoder(renderer) != 1) {
        free(renderer);
        renderer = NULL;
        return NULL;
    }

    if (audio_renderer_rpi_init_renderer(renderer, video_renderer) != 1) {
        audio_renderer_alsa_destroy_decoder(renderer);
        free(renderer);
        renderer = NULL;
        return NULL;
    }

    return &renderer->base;
}

static void audio_renderer_alsa_start(__attribute__((unused)) audio_renderer_t *renderer) {
    // Nothing to do
}

#ifdef DUMP_AUDIO
static FILE* file_pcm = NULL;
#endif

static void audio_renderer_alsa_render_buffer( audio_renderer_t *renderer,
                                               __attribute__((unused)) raop_ntp_t *ntp,
                                               unsigned char *data,
                                               int data_len,
                                               __attribute__((unused)) uint64_t pts )
{
    if (data_len == 0) return;
    
    audio_renderer_alsa_t *r = (audio_renderer_alsa_t *)renderer;

    logger_log(renderer->logger, LOGGER_DEBUG, "Got AAC data of %d bytes", data_len);

    // We assume that every buffer contains exactly 1 frame.

    AAC_DECODER_ERROR error;

    UCHAR *p_buffer[1] = {data};
    UINT buffer_size = data_len;
    UINT bytes_valid = data_len;
    error = aacDecoder_Fill(r->audio_decoder, p_buffer, &buffer_size, &bytes_valid);
    if (error != AAC_DEC_OK) {
        logger_log(renderer->logger, LOGGER_ERR, "aacDecoder_Fill error : %x", error);
    }

    INT time_data_size = 4 * 480;
    INT_PCM *p_time_data = malloc(time_data_size); // The buffer for the decoded AAC frames
    error = aacDecoder_DecodeFrame(r->audio_decoder, p_time_data, time_data_size, 0);
    if (error != AAC_DEC_OK) {
        logger_log(renderer->logger, LOGGER_ERR, "aacDecoder_DecodeFrame error : 0x%x", error);
    }

#ifdef DUMP_AUDIO
    if (file_pcm == NULL) {
        file_pcm = fopen("/home/pi/Airplay.pcm", "wb");
    }

    fwrite(p_time_data, time_data_size, 1, file_pcm);
#endif

    snd_pcm_sframes_t playedFrames, receivedFrames = snd_pcm_bytes_to_frames(r->audio_renderer,time_data_size);
    playedFrames = snd_pcm_writei(r->audio_renderer,p_time_data , receivedFrames);
    if (playedFrames < 0)
        playedFrames = snd_pcm_recover(r->audio_renderer, (int)playedFrames, 0);
    if (playedFrames < 0)
        logger_log(renderer->logger, LOGGER_ERR, snd_strerror((int) playedFrames));

    if (playedFrames > 0 && playedFrames < receivedFrames)
        logger_log( renderer->logger,
                    LOGGER_ERR,
                    "ALSA: Expected write %li, but wrote %li frames)\n", receivedFrames, playedFrames);

    free(p_time_data);
}

static void audio_renderer_alsa_set_volume(audio_renderer_t *renderer, float volume) {
    if ( !mHasCtrlVolume ) return;

    audio_renderer_alsa_t *r = (audio_renderer_alsa_t *)renderer;
    long raw;
    CHK_ALSA_ERRNO (
            snd_ctl_convert_from_dB(r->ctl, r->ctl_elem_id, (long)volume * 200, &raw, 0),
            "Convert",
            r->base.logger );
    snd_ctl_elem_value_t *value;
    snd_ctl_elem_value_alloca(&value);
    snd_ctl_elem_value_set_id(value, r->ctl_elem_id);
    snd_ctl_elem_value_set_integer(value, 0, raw);
    snd_ctl_elem_value_set_integer(value, 1, raw);
    CHK_ALSA_ERRNO( snd_ctl_elem_write(r->ctl, value), "Set volume", r->base.logger );
}

static void audio_renderer_alsa_flush(__attribute__((unused)) audio_renderer_t *renderer) {
    // Nothing to do
}

static void audio_renderer_alsa_destroy(audio_renderer_t *renderer) {
    if (renderer) {
        audio_renderer_alsa_t *r = (audio_renderer_alsa_t *)renderer;
        audio_renderer_alsa_flush(renderer);
        audio_renderer_alsa_destroy_decoder(r);
        audio_renderer_rpi_destroy_renderer(r);
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
