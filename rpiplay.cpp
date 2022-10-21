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

#include <stddef.h>
#include <cstring>
#include <signal.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <fstream>

#include <sys/socket.h>
#include <ifaddrs.h>
#ifdef __linux__
#include <netpacket/packet.h>
#else
#include <net/if_dl.h>   /* macOS and *BSD */
#endif

#include "log.h"
#include "lib/raop.h"
#include "lib/stream.h"
#include "lib/logger.h"
#include "lib/dnssd.h"
#include "renderers/video_renderer.h"
#include "renderers/audio_renderer.h"

#define VERSION "1.2"

#if defined(__x86_64__)
    #define DEFAULT_NAME "RPiPlayPC"
#else
    #define DEFAULT_NAME "RPiPlayNOTPC"
#endif 

#define DEFAULT_BACKGROUND_MODE BACKGROUND_MODE_ON
#define DEFAULT_AUDIO_DEVICE AUDIO_DEVICE_HDMI
#define DEFAULT_LOW_LATENCY false
#define DEFAULT_DEBUG_LOG false
#define DEFAULT_ROTATE 0
#define DEFAULT_FLIP FLIP_NONE
#define DEFAULT_HW_ADDRESS { (char) 0x48, (char) 0x5d, (char) 0x60, (char) 0x7c, (char) 0xee, (char) 0x22 }

int start_server(std::vector<char> hw_addr, std::string name, bool debug_log,
                 video_renderer_config_t const *video_config, audio_renderer_config_t const *audio_config);

int stop_server();

typedef video_renderer_t *(*video_init_func_t)(logger_t *logger, video_renderer_config_t const *config);
typedef audio_renderer_t *(*audio_init_func_t)(logger_t *logger, video_renderer_t *video_renderer, audio_renderer_config_t const *config);

typedef struct video_renderer_list_entry_s {
    const char *name;
    const char *description;
    video_init_func_t init_func;
} video_renderer_list_entry_t;

typedef struct audio_renderer_list_entry_s {
    const char *name;
    const char *description;
    audio_init_func_t init_func;
} audio_renderer_list_entry_t;

static bool running = false;
static dnssd_t *dnssd = NULL;
static raop_t *raop = NULL;
static video_init_func_t video_init_func = NULL;
static audio_init_func_t audio_init_func = NULL;
static video_renderer_t *video_renderer = NULL;
static audio_renderer_t *audio_renderer = NULL;
static logger_t *render_logger = NULL;

static const video_renderer_list_entry_t video_renderers[] = {
#if defined(HAS_RPI_RENDERER)
    {"rpi", "Raspberry Pi OpenMAX accelerated H.264 renderer", video_renderer_rpi_init},
#endif
#if defined(HAS_GSTREAMER_RENDERER)
    {"gstreamer", "GStreamer H.264 renderer", video_renderer_gstreamer_init},
#endif
#if defined(HAS_DUMMY_RENDERER)
    {"dummy", "Dummy renderer; does not actually display video", video_renderer_dummy_init},
#endif
};

static const audio_renderer_list_entry_t audio_renderers[] = {
#if defined(HAS_RPI_RENDERER)
    {"rpi", "AAC renderer using fdk-aac for decoding and OpenMAX for rendering", audio_renderer_rpi_init},
#endif
#if defined(HAS_GSTREAMER_RENDERER)
    {"gstreamer", "GStreamer audio renderer", audio_renderer_gstreamer_init},
#endif
#if defined(HAS_DUMMY_RENDERER)
    {"dummy", "Dummy renderer; does not actually play audio", audio_renderer_dummy_init},
#endif
};

static void signal_handler(int sig) {
    switch (sig) {
        case SIGINT:
        case SIGTERM:
            running = 0;
            break;
    }
}

static void init_signals(void) {
    //struct sigaction sigact;
    //sigact.sa_handler = signal_handler;
    //sigemptyset(&sigact.sa_mask);
    //sigact.sa_flags = 0;
    //sigaction(SIGINT, &sigact, NULL);
    //sigaction(SIGTERM, &sigact, NULL);
}

static int parse_hw_addr(std::string str, std::vector<char> &hw_addr) {
    for (int i = 0; i < str.length(); i += 3) {
        hw_addr.push_back((char) stol(str.substr(i), NULL, 16));
    }
    return 0;
}

static std::string find_mac () {
/*  finds the MAC address of the first active network interface *
 *  in a Linux, *BSD or macOS system.                           */
    std::string mac_address = "";
    struct ifaddrs *ifap, *ifaptr;
    int non_null_octets = 0;
    unsigned char octet[6], *ptr;
    if (getifaddrs(&ifap) == 0) {
        for(ifaptr = ifap; ifaptr != NULL; ifaptr = ifaptr->ifa_next) {
            if(ifaptr->ifa_addr == NULL) continue;
#ifdef __linux__
            if (ifaptr->ifa_addr->sa_family != AF_PACKET) continue;
            struct sockaddr_ll *s = (struct sockaddr_ll*) ifaptr->ifa_addr;
            for (int i = 0; i < 6; i++) {
                if ((octet[i] = s->sll_addr[i]) != 0) non_null_octets++;
            }
#else    /* macOS and *BSD */
            if (ifaptr->ifa_addr->sa_family != AF_LINK) continue;
            ptr = (unsigned char *) LLADDR((struct sockaddr_dl *) ifaptr->ifa_addr);
            for (int i= 0; i < 6 ; i++) {
                if ((octet[i] = *ptr) != 0) non_null_octets++;
                ptr++;
            }
#endif
            if (non_null_octets) {
                mac_address.erase();
                char str[3];
                for (int i = 0; i < 6 ; i++) {
                    sprintf(str,"%02x", octet[i]);
                    mac_address = mac_address + str;
                    if (i < 5) mac_address = mac_address + ":";
                }
                break;
            }
        }
    }
    freeifaddrs(ifap);
    return mac_address;
}

static video_init_func_t find_video_init_func(const char *name) {
    for (int i = 0; i < sizeof(video_renderers)/sizeof(video_renderers[0]); i++) {
        if (!strcmp(name, video_renderers[i].name)) {
            return video_renderers[i].init_func;
        }
    }
    return NULL;
}

static audio_init_func_t find_audio_init_func(const char *name) {
    for (int i = 0; i < sizeof(audio_renderers)/sizeof(audio_renderers[0]); i++) {
        if (!strcmp(name, audio_renderers[i].name)) {
            return audio_renderers[i].init_func;
        }
    }
    return NULL;
}

void print_info(char *name) {
    printf("RPiPlay %s: An open-source AirPlay mirroring server for Raspberry Pi\n", VERSION);
    printf("Usage: %s [-n name] [-b (on|auto|off)] [-r (90|180|270)] [-l] [-a (hdmi|analog|off)] [-vr renderer] [-ar renderer]\n", name);
    printf("Options:\n");
    printf("-n name               Specify the network name of the AirPlay server\n");
    printf("-b (on|auto|off)      Show black background always, only during active connection, or never\n");
    printf("-r (90|180|270)       Specify image rotation in multiples of 90 degrees\n");
    printf("-f (horiz|vert|both)  Specify image flipping (horiz = horizontal, vert = vertical, both = both)\n");
    printf("-l                    Enable low-latency mode (disables render clock)\n");
    printf("-a (hdmi|analog|off)  Set audio output device\n");
    printf("-vr renderer          Set video renderer to use. Available renderers:\n");
    for (int i = 0; i < sizeof(video_renderers)/sizeof(video_renderers[0]); i++) {
        printf("    %s: %s%s\n", video_renderers[i].name, video_renderers[i].description, i == 0 ? " [Default]" : "");
    }
    printf("-ar renderer          Set audio renderer to use. Available renderers:\n");
    for (int i = 0; i < sizeof(audio_renderers)/sizeof(audio_renderers[0]); i++) {
        printf("    %s: %s%s\n", audio_renderers[i].name, audio_renderers[i].description, i == 0 ? " [Default]" : "");
    }
    printf("-d                    Enable debug logging\n");
    printf("-v/-h                 Displays this help and version information\n");
}

int main(int argc, char *argv[]) {
    init_signals();
    
    std::string server_name = DEFAULT_NAME;
    std::vector<char> server_hw_addr = DEFAULT_HW_ADDRESS;
    bool debug_log = DEFAULT_DEBUG_LOG;

    video_renderer_config_t video_config;
    video_config.background_mode = DEFAULT_BACKGROUND_MODE;
    video_config.low_latency = DEFAULT_LOW_LATENCY;
    video_config.rotation = DEFAULT_ROTATE;
    video_config.flip = DEFAULT_FLIP;
    
    audio_renderer_config_t audio_config;
    audio_config.device = DEFAULT_AUDIO_DEVICE;
    audio_config.low_latency = DEFAULT_LOW_LATENCY;
    
    // Default to the best available renderer
    video_init_func = video_renderers[0].init_func;
    audio_init_func = audio_renderers[0].init_func;

    // Parse arguments
    for (int i = 1; i < argc; i++) {
        std::string arg(argv[i]);
        if (arg == "-n") {
            if (i == argc - 1) continue;
            server_name = std::string(argv[++i]);
        } else if (arg == "-b") {
            // For backwards-compatibility, make just -b disable the background
            if (i == argc - 1 || argv[i + 1][0] == '-') {
                video_config.background_mode = BACKGROUND_MODE_OFF;
                continue;
            }

            std::string background_mode(argv[++i]);
            video_config.background_mode = background_mode == "off" ? BACKGROUND_MODE_OFF :
                                           background_mode == "auto" ? BACKGROUND_MODE_AUTO :
                                           BACKGROUND_MODE_ON;
        } else if (arg == "-a") {
            if (i == argc - 1) continue;
            std::string audio_device_name(argv[++i]);
            audio_config.device = audio_device_name == "hdmi" ? AUDIO_DEVICE_HDMI :
                                  audio_device_name == "analog" ? AUDIO_DEVICE_ANALOG :
                                  AUDIO_DEVICE_NONE;
        } else if (arg == "-l") {
            video_config.low_latency = !video_config.low_latency;
            audio_config.low_latency = !audio_config.low_latency;
        } else if (arg == "-r") {
            video_config.rotation = atoi(argv[++i]);
        } else if (arg == "-f") {
            if (i == argc - 1) continue;
            std::string flip_type(argv[++i]);
            video_config.flip = flip_type == "horiz" ? FLIP_HORIZONTAL :
                                flip_type == "vert" ? FLIP_VERTICAL :
                                flip_type == "both" ? FLIP_BOTH :
                                FLIP_NONE;
        } else if (arg == "-d") {
            debug_log = !debug_log;
        } else if (arg == "-vr") {
            if (i == argc - 1) {
                fprintf(stderr, "Error: You must supply the name of a video renderer after the -vr argument.\n");
                exit(1);
            }
            video_init_func = find_video_init_func(argv[++i]);
            if (!video_init_func) {
                fprintf(stderr, "Error: Unable to locate video renderer \"%s\".\n", argv[i]);
                exit(1);
            }
        } else if (arg == "-ar") {
            if (i == argc - 1) {
                fprintf(stderr, "Error: You must supply the name of an audio renderer after the -ar argument.\n");
                exit(1);
            }
            audio_init_func = find_audio_init_func(argv[++i]);
            if (!audio_init_func) {
                fprintf(stderr, "Error: Unable to locate audio renderer \"%s\".\n", argv[i]);
                exit(1);
            }
        } else if (arg == "-h" || arg == "-v") {
            print_info(argv[0]);
            exit(0);
        }
    }

    std::string mac_address = find_mac();
    if (!mac_address.empty()) {
        server_hw_addr.clear();
        parse_hw_addr(mac_address, server_hw_addr);
    }

    if (start_server(server_hw_addr, server_name, debug_log, &video_config, &audio_config) != 0) {
        return 1;
    }

    LOGI("Running...");
    running = true;
    while (running) {
        sleep(1);
    }

    LOGI("Stopping...");
    stop_server();

    LOGI("Done.");

    exit(0);
}

// Server callbacks
extern "C" void conn_init(void *cls) {
    LOGI("conn_init");

    if (video_renderer) {
        video_renderer->funcs->conn_init(video_renderer);
        video_renderer->funcs->update_background(video_renderer, 1);
    }
}

extern "C" void conn_destroy(void *cls) {
    LOGI("conn_destroy");

    if (video_renderer) {
        video_renderer->funcs->conn_destroy(video_renderer);
        video_renderer->funcs->update_background(video_renderer, -1);
    }
}

extern "C" void audio_process(void *cls, raop_ntp_t *ntp, aac_decode_struct *data) {
    if (audio_renderer != NULL) {
        audio_renderer->funcs->render_buffer(audio_renderer, ntp, data->data, data->data_len, data->pts);
    }
}

extern "C" void video_process(void *cls, raop_ntp_t *ntp, h264_decode_struct *data) {
    if (video_renderer != NULL) {
        video_renderer->funcs->render_buffer(video_renderer, ntp, data->data, data->data_len, data->pts, data->frame_type);
    }
}

extern "C" void audio_flush(void *cls) {
    if (audio_renderer) audio_renderer->funcs->flush(audio_renderer);
}

extern "C" void video_flush(void *cls) {
    if (video_renderer) video_renderer->funcs->flush(video_renderer);
}

extern "C" void audio_set_volume(void *cls, float volume) {
    if (audio_renderer != NULL) {
        audio_renderer->funcs->set_volume(audio_renderer, volume);
    }
}

extern "C" void log_callback(void *cls, int level, const char *msg) {
    switch (level) {
        case LOGGER_DEBUG: {
            LOGD("%s", msg);
            break;
        }
        case LOGGER_WARNING: {
            LOGW("%s", msg);
            break;
        }
        case LOGGER_INFO: {
            LOGI("%s", msg);
            break;
        }
        case LOGGER_ERR: {
            LOGE("%s", msg);
            break;
        }
        default:
            break;
    }

}

int start_server(std::vector<char> hw_addr, std::string name, bool debug_log,
                 video_renderer_config_t const *video_config, audio_renderer_config_t const *audio_config) {
    raop_callbacks_t raop_cbs;
    memset(&raop_cbs, 0, sizeof(raop_cbs));
    raop_cbs.conn_init = conn_init;
    raop_cbs.conn_destroy = conn_destroy;
    raop_cbs.audio_process = audio_process;
    raop_cbs.video_process = video_process;
    raop_cbs.audio_flush = audio_flush;
    raop_cbs.video_flush = video_flush;
    raop_cbs.audio_set_volume = audio_set_volume;

    raop = raop_init(10, &raop_cbs);
    if (raop == NULL) {
        LOGE("Error initializing raop!");
        return -1;
    }

    raop_set_log_callback(raop, log_callback, NULL);
    raop_set_log_level(raop, debug_log ? RAOP_LOG_DEBUG : LOGGER_INFO);

    render_logger = logger_init();
    logger_set_callback(render_logger, log_callback, NULL);
    logger_set_level(render_logger, debug_log ? LOGGER_DEBUG : LOGGER_INFO);

    if (video_config->low_latency) logger_log(render_logger, LOGGER_INFO, "Using low-latency mode");

    if ((video_renderer = video_init_func(render_logger, video_config)) == NULL) {
        LOGE("Could not init video renderer");
        return -1;
    }

    if (audio_config->device == AUDIO_DEVICE_NONE) {
        LOGI("Audio disabled");
    } else if ((audio_renderer = audio_init_func(render_logger, video_renderer, audio_config)) ==
               NULL) {
        LOGE("Could not init audio renderer");
        return -1;
    }

    if (video_renderer) video_renderer->funcs->start(video_renderer);
    if (audio_renderer) audio_renderer->funcs->start(audio_renderer);

    unsigned short port = 0;
    raop_start(raop, &port);
    raop_set_port(raop, port);

    int error;
    dnssd = dnssd_init(name.c_str(), strlen(name.c_str()), hw_addr.data(), hw_addr.size(), &error);
    if (error) {
        LOGE("Could not initialize dnssd library!");
        return -2;
    }

    raop_set_dnssd(raop, dnssd);

    dnssd_register_raop(dnssd, port);
    dnssd_register_airplay(dnssd, port + 1);

    return 0;
}

int stop_server() {
    raop_destroy(raop);
    dnssd_unregister_raop(dnssd);
    dnssd_unregister_airplay(dnssd);
    // If we don't destroy these two in the correct order, we get a deadlock from the ilclient library
    if (audio_renderer) audio_renderer->funcs->destroy(audio_renderer);
    if (video_renderer) video_renderer->funcs->destroy(video_renderer);
    logger_destroy(render_logger);
    return 0;
}
