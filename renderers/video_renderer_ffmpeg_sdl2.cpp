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

#include "sdl_event.h"
#include "video_renderer.h"

#include <SDL.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <future>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/error.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

static char av_error[AV_ERROR_MAX_STRING_SIZE] = {0};
#undef av_err2str
#define av_err2str(errnum)                                                     \
  av_make_error_string(av_error, AV_ERROR_MAX_STRING_SIZE, errnum)

typedef struct video_renderer_ffmpeg_sdl2_s {
  video_renderer_t base;
} video_renderer_ffmpeg_sdl2_t;

static AVCodecContext *codec_ctx;
static SDL_Renderer *sdl_renderer;
static SDL_Window *sdl_window;
static SDL_Texture *sdl_texture;
static AVFrame *av_frame;
static AVPacket *av_packet;
static SwsContext *sws_ctx;
static AVFrame *av_frame_render_yuv;
static uint8_t *av_frame_render_yuv_buffer;

static struct {
  int w = 100;
  int h = 100;
} window_size;

static void reinit_scale(logger_t *logger) {
  // sdl_texture
  {
    if (sdl_texture) {
      SDL_DestroyTexture(sdl_texture);
    }
    sdl_texture = SDL_CreateTexture(sdl_renderer, SDL_PIXELFORMAT_IYUV,
                                    SDL_TEXTUREACCESS_STREAMING, window_size.w,
                                    window_size.h);
  }

  // av_frame_render_yuv
  {
    if (av_frame_render_yuv) {
      av_frame_free(&av_frame_render_yuv);
    }
    av_frame_render_yuv = av_frame_alloc();

    if (av_frame_render_yuv_buffer) {
      av_free(av_frame_render_yuv_buffer);
    }
    int buf_size = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, window_size.w,
                                            window_size.h, 1);
    av_frame_render_yuv_buffer = (uint8_t *)av_malloc(buf_size);
    av_image_fill_arrays(av_frame_render_yuv->data,     // dst data[]
                         av_frame_render_yuv->linesize, // dst linesize[]
                         av_frame_render_yuv_buffer,    // src buffer
                         AV_PIX_FMT_YUV420P,            // pixel format
                         window_size.w,                 // width
                         window_size.h,                 // height
                         1                              // align
    );
  }

  // sws_ctx
  {
    if (sws_ctx) {
      sws_freeContext(sws_ctx);
    }
    sws_ctx = sws_getContext(codec_ctx->width,   // src width
                             codec_ctx->height,  // src height
                             codec_ctx->pix_fmt, // src format
                             window_size.w,      // dst width
                             window_size.h,      // dst height
                             AV_PIX_FMT_YUV420P, // dst format
                             SWS_BICUBIC,        // flags
                             nullptr,            // src filter
                             nullptr,            // dst filter
                             nullptr             // param
    );
    if (sws_ctx == nullptr) {
      logger_log(logger, LOGGER_ERR, "sws_getContext() failed");
      SDL_Quit();
    }
  }
}

static void video_renderer_ffmpeg_sdl2_start(video_renderer_t *renderer) {}

// #define DEBUG_H264_FILE
#ifdef DEBUG_H264_FILE
static std::ofstream video_file("video.h264", std::ios::app | std::ios::binary);
#endif
static void video_renderer_ffmpeg_sdl2_render_buffer(video_renderer_t *renderer,
                                                     raop_ntp_t *ntp,
                                                     unsigned char *h264buffer,
                                                     int h264buffer_size,
                                                     uint64_t pts, int type) {
#ifdef DEBUG_H264_FILE
  video_file.write(reinterpret_cast<char *>(h264buffer), h264buffer_size);
#endif
  logger_log(renderer->logger, LOGGER_DEBUG, "render_buffer: %d",
             h264buffer_size);
  AVPacket *packet = av_packet;
  packet->pts = (int64_t)pts;
  packet->data = h264buffer;
  packet->size = h264buffer_size;
  AVFrame *frame = av_frame;

  int ret = avcodec_send_packet(codec_ctx, packet);
  while (ret >= 0) {
    ret = avcodec_receive_frame(codec_ctx, frame);
    if (ret == 0) {
      int w = frame->width;
      int h = frame->height;
      logger_log(renderer->logger, LOGGER_DEBUG, "frame: %d, %d", w, h);

      std::promise<void> promise;
      SDL_Event event;
      event.type = SDL_USER_FUNC;
      event.user.data1 = new std::function<void()>([&]() mutable {
        // auto resize window
        if (window_size.w != w || window_size.h != h) {
          window_size.w = w;
          window_size.h = h;
          reinit_scale(renderer->logger);
          SDL_SetWindowSize(sdl_window, w, h);
          // SDL_RenderSetLogicalSize(sdl_renderer, w, h); // will keep ratio
        }

        sws_scale(sws_ctx,                      // sws context
                  frame->data,                  // src slice
                  frame->linesize,              // src stride
                  0,                            // src slice y
                  frame->height,                // src slice height
                  av_frame_render_yuv->data,    // dst planes
                  av_frame_render_yuv->linesize // dst strides
        );

        // render on main thread
        SDL_UpdateYUVTexture(
            sdl_texture, nullptr, av_frame_render_yuv->data[0],
            av_frame_render_yuv->linesize[0], av_frame_render_yuv->data[1],
            av_frame_render_yuv->linesize[1], av_frame_render_yuv->data[2],
            av_frame_render_yuv->linesize[2]);
        SDL_RenderClear(sdl_renderer);
        SDL_RenderCopy(sdl_renderer, sdl_texture, nullptr, nullptr);
        SDL_RenderPresent(sdl_renderer);
        promise.set_value();
      });
      SDL_PushEvent(&event);
      promise.get_future().get();
    }
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
      break;
    else if (ret < 0) {
      logger_log(renderer->logger, LOGGER_ERR, "Error decoding frame: %s",
                 av_err2str(ret));
      SDL_Quit();
    }
  }
}

static void video_renderer_ffmpeg_sdl2_flush(video_renderer_t *renderer) {}

static void video_renderer_ffmpeg_sdl2_destroy(video_renderer_t *renderer) {
  if (renderer) {
    free(renderer);
  }
  av_frame_free(&av_frame);
  av_packet_unref(av_packet);
  avcodec_free_context(&codec_ctx);
  SDL_DestroyTexture(sdl_texture);
  SDL_DestroyRenderer(sdl_renderer);
  SDL_DestroyWindow(sdl_window);
  SDL_Quit();
}

static void
video_renderer_ffmpeg_sdl2_update_background(video_renderer_t *renderer,
                                             int type) {}

static const video_renderer_funcs_t video_renderer_ffmpeg_sdl2_funcs = {
    .start = video_renderer_ffmpeg_sdl2_start,
    .render_buffer = video_renderer_ffmpeg_sdl2_render_buffer,
    .flush = video_renderer_ffmpeg_sdl2_flush,
    .destroy = video_renderer_ffmpeg_sdl2_destroy,
    .update_background = video_renderer_ffmpeg_sdl2_update_background,
};

video_renderer_t *
video_renderer_ffmpeg_sdl2_init(logger_t *logger,
                                video_renderer_config_t const *config) {
  // init sdl
  {
    SDL_Init(SDL_INIT_VIDEO);
    int flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
    sdl_window = SDL_CreateWindow("H264 Player", SDL_WINDOWPOS_UNDEFINED,
                                  SDL_WINDOWPOS_UNDEFINED, window_size.w,
                                  window_size.h, flags);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

    sdl_renderer = SDL_CreateRenderer(
        sdl_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  }

  // init av
  {
    av_frame = av_frame_alloc();
    av_packet = av_packet_alloc();
  }

  // codec
  {
    const AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    codec_ctx = avcodec_alloc_context3(codec);
    codec_ctx->time_base = {1, 25};
    codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    codec_ctx->bit_rate = 0;
    avcodec_open2(codec_ctx, codec, nullptr);
  }

  // init renderer
  video_renderer_ffmpeg_sdl2_t *renderer;
  renderer = (video_renderer_ffmpeg_sdl2_t *)calloc(
      1, sizeof(video_renderer_ffmpeg_sdl2_t));
  renderer->base.logger = logger;
  renderer->base.funcs = &video_renderer_ffmpeg_sdl2_funcs;
  renderer->base.type = VIDEO_RENDERER_FFMPEG_SDL2;
  return &renderer->base;
}
