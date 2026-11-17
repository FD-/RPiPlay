# AGENTS.md

C/C++ (C++11) AirPlay mirroring server for Raspberry Pi. No tests, no CI, no lint config — **building the project is the only verification available**.

## Build

Out-of-tree CMake; default build type is Release:

```bash
mkdir -p build && cd build && cmake .. && make -j
```

Deps (Debian/Ubuntu): `cmake libavahi-compat-libdnssd-dev libplist-dev libssl-dev libasound2-dev gstreamer1.0 libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev` (full lists in README.md). OpenSSL >= 1.1.1 required (`lib/CMakeLists.txt` sets `OPENSSL_API_COMPAT=0x10101000L`); on macOS it hardcodes `/usr/local/opt/openssl@1.1`.

## Conditional renderer compilation (non-obvious)

- `renderers/CMakeLists.txt` detects backends at **configure time**: Broadcom OpenMAX libs in `/opt/vc/` (Raspberry Pi only), GStreamer via pkg-config, and ALSA via `find_package(ALSA)`. Dummy renderers always build.
- It communicates results by appending `-DHAS_RPI_RENDERER` / `-DHAS_ALSA_RENDERER` / `-DHAS_GSTREAMER_RENDERER` / `-DHAS_DUMMY_RENDERER` to the `RENDERER_FLAGS` parent-scope variable, which the root CMakeLists injects into global CXX flags; `rpiplay.cpp` gates renderer registration on these `#if defined(...)` blocks.
- **Adding a new renderer** means touching both `renderers/CMakeLists.txt` (sources, link libs, new `HAS_*` define) and the `#ifdef` blocks in `rpiplay.cpp`. On desktop Linux without GStreamer dev packages installed, only dummy (plus ALSA if `libasound2-dev` is present) renderers build — don't misread missing renderers as code errors; check the CMake configure output.
- The vendored `fdk-aac` is only added as a subdirectory when the RPI **or** ALSA renderer is detected (`NEED_FDK_AAC`); both audio renderers decode the AAC-ELD mirror stream themselves.
- The ALSA renderer exists because the OMX `audio_render` component can only output to "local"/"hdmi" — external I2S DACs (HiFiBerry) are only reachable through ALSA. With `-a analog` it auto-selects a HiFiBerry card; `-a` also accepts explicit ALSA device names (e.g. `sysdefault:CARD=sndrpihifiberrydacplus`).
- GStreamer renderer modern-Pi behaviour: video decoder auto-selects `v4l2h264dec` (Pi HW decode, bcm2835_codec) vs `decodebin` (software / Pi 5); `RPIPLAY_VDECODER`/`RPIPLAY_VIDEOSINK`/`RPIPLAY_AUDIOSINK`/`RPIPLAY_BT709` env vars override. On Pi 5 (no OpenMAX, no HW H.264) gstreamer is the only renderer.
- Building locally on Ubuntu 24.04: `gstreamer-1.0.pc` requires `libunwind`, but `libunwind-18-dev` ships no `.pc` — pkg-config then fails on the whole gstreamer chain. Workaround: a user-local `libunwind.pc` in a dir passed via `PKG_CONFIG_PATH` (or `sudo apt-get install -y libunwind-dev`).
- `deploy/` contains the full provisioning kit for the reference deployment (Pi Zero + InnoMaker PCM5122 DAC): `provision.sh`, a patched `snd-soc-pcm512x` kernel module (Raspberry Pi OS kernels ≥ 6.12 fail PCM512x opens with -22 via the runtime-PM path — see `deploy/README.md`), systemd units, and BT pairing agent. The reference config uses the `allo-boss-dac-pcm512x-audio` overlay (same PCM5122 at I2C 0x4D, dummy regulators, codec-master) — the `hifiberry-dacplus*` overlays hit the -22 failure and a Pi-master clocking mismatch on this board. Keep HDMI cables unplugged on this stack: the connector shell shorts HAT header pins under contact.

## Vendored third-party code — don't reformat or casually edit

- `lib/playfair/` (GPL FairPlay), `lib/llhttp/` (MIT), `renderers/h264-bitstream/`, `renderers/fdk-aac/` (upstream clone, only built when the RPi renderer is found).
- Actual RPiPlay code: `lib/` (AirPlay protocol: raop*, httpd, pairing, dnssd, crypto — largely ported from dsafa22/shairplay, LGPL), `renderers/*.c` (video/audio renderer pairs: rpi, gstreamer, dummy), `rpiplay.cpp` (CLI entrypoint wiring `-vr`/`-ar` renderer selection).

## Gotchas

- `//#define DUMP_AUDIO` / `DUMP_H264` / `DUMP_KEY_IV` blocks exist in `lib/` and `renderers/audio_renderer_rpi.c` for dumping raw streams — keep them disabled; they destroy real-time performance.
- On x86, `renderers/CMakeLists.txt` force-adds `-Ofast -march=native` to C flags.
- The `-b`, `-r`, `-l`, `-a` CLI options are unsupported with the gstreamer renderer.
