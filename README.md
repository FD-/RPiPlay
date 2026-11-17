# Introduction

An open-source implementation of an AirPlay mirroring server for the Raspberry Pi.
The goal is to make it run smoothly even on a Raspberry Pi Zero.


# Quickstart

Build and start `rpiplay` — an AirPlay target appears on your network under the
name of the server (default "RPiPlay"):

```bash
rpiplay
```

> **Building a complete Bluetooth-speaker + AirPlay box with a DAC HAT?**
> Skip the manual work: `deploy/provision.sh` sets everything up in one run.
> See [deploy/README.md](deploy/README.md) for the automated quickstart and a
> full step-by-step manual setup (packages, DAC overlay, patched driver,
> services, phone pairing).

Both devices must be on the same network (and mDNS/Bonjour traffic must be
allowed — check your router's "AP isolation" setting if the server does not
show up).

## iPhone / iPad

1. Open **Control Center → Screen Mirroring** and select **RPiPlay**.
2. Screen (with its audio) mirrors to the Pi — e.g. play YouTube Music or any
   other app while mirroring; its audio is streamed to the Pi automatically.

Notes:

* Use *Screen Mirroring*, not the in-app AirPlay icon: audio-only AirPlay
  (ALAC) streams are not supported.
* Alternatively, pair the Pi in the Bluetooth settings and use it as an A2DP
  speaker — no mirroring needed (the Pi must have Bluetooth audio set up, see
  the Bluetooth section below).

## Android

Android has no built-in AirPlay support, so pick one of these:

* **Bluetooth (recommended, no app needed)** — pair the Pi in the Bluetooth
  settings and play any music app; the Pi acts as an A2DP speaker. The Pi-side
  setup is described in the Bluetooth section below.
* **A mirroring sender app** — install an app that discovers AirPlay receivers
  (e.g. "Cast to TV" or LetsView), select RPiPlay and mirror. Support varies
  by app and vendor; test before relying on it.
* Samsung **Smart View does not work** — it speaks Miracast/Google Cast/DLNA,
  not AirPlay.

Audio on the Pi itself requires an audio output — see the external DAC section
below for HiFiBerry boards.


# State

Screen mirroring and audio works for iOS 9 or newer. Recent macOS versions also seem to be compatible. The GPU is used for decoding the h264 video stream. The Pi has no hardware acceleration for audio (AirPlay mirroring uses AAC), so the FDK-AAC decoder is used for that.

Both audio and video work fine on a Raspberry Pi 3B+ and a Raspberry Pi Zero, though playback is a bit smoother on the 3B+.

For best performance:
* Use a wired network connection
* Compile with -O3 (cmake --DCMAKE_CXX_FLAGS="-O3" --DCMAKE_C_FLAGS="-O3" ..)
* Make sure the DUMP flags are *not* active
* Make sure you *don't* use the -d debug log flag
* Make sure no other demanding tasks are running (this is particularly important for audio on the Pi Zero)

By using OpenSSL for AES decryption, I was able to speed up the decryption of video packets from up to 0.2 seconds to up to 0.007 seconds for large packets (On the Pi Zero). Average is now more like 0.002 seconds.

There still are some minor issues. Have a look at the TODO list below.

RPiPlay might not be suitable for remote video playback, as it lacks a dedicated component for that: It seems like AirPlay on an AppleTV effectively runs a web server on the device and sends the URL to the AppleTV, thus avoiding the re-encoding of the video.
For rough details, refer to the (mostly obsolete) [inofficial AirPlay specification](https://nto.github.io/AirPlay.html#screenmirroring).



# Building

The following packages are required for building on Raspbian:

* **cmake** (for the build system)
* **libavahi-compat-libdnssd-dev** (for the bonjour registration)
* **libplist-dev** (for plist handling)
* **libssl-dev** (for crypto primitives)
* **libasound2-dev** (for the ALSA audio renderer)
* **ilclient** and Broadcom's OpenMAX stack as present in `/opt/vc` in Raspbian.

For downloading the code, use these commands:
```bash
git clone https://github.com/FD-/RPiPlay.git
cd RPiPlay
```

For building on a fresh Raspbian Stretch or Buster install, these steps should be run:
```bash
sudo apt-get install cmake
sudo apt-get install libavahi-compat-libdnssd-dev
sudo apt-get install libplist-dev
sudo apt-get install libssl-dev
sudo apt-get install libasound2-dev
mkdir build
cd build
cmake ..
make -j
```

GCC 5 or later is required.

# Building on desktop Linux:

For building on desktop linux, follow these steps as per your distribution:

## Ubuntu 18.04 or 20.04
```bash
sudo apt-get install cmake libavahi-compat-libdnssd-dev libplist-dev libssl-dev libasound2-dev \
    libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev gstreamer1.0-libav \
    gstreamer1.0-vaapi gstreamer1.0-plugins-bad
mkdir build
cd build
cmake ..
make
```

## Fedora 33
```bash
sudo dnf install cmake avahi-compat-libdns_sd-devel libplist-devel openssl-devel alsa-lib-devel \
    gstreamer1-plugins-base-devel gstreamer1-libav gstreamer1-vaapi \
    gstreamer1-plugins-bad-free
mkdir build
cd build
cmake ..
make
```

Note: The -b, -r, -l and -a options are not supported with the gstreamer renderer.

# Modern Raspberry Pi systems (Pi 4, Pi 5, 64-bit OS)

The OpenMAX-based Raspberry Pi renderer only works on Pi models whose firmware
still ships the Broadcom OpenMAX stack (`/opt/vc`): Pi 3/4 and the Pi Zero
family, 32- or 64-bit OS. Use the GStreamer renderers on everything else:

* **Raspberry Pi 5**: OpenMAX is gone and there is no hardware H.264 decoder —
  the CPU is fast enough for software decoding. Use `-vr gstreamer -ar gstreamer`.
* **Pi 4 / Pi 3 / Pi Zero 2 W on Raspberry Pi OS Bookworm (GStreamer >= 1.22)**:
  the GStreamer video renderer automatically uses hardware H.264 decoding via
  the `v4l2h264dec` element (needs the `bcm2835_codec` kernel module and
  `gstreamer1.0-plugins-good`). Check the startup log: it prints
  `GStreamer video decoder: v4l2h264dec` when the hardware path is active.
* **Headless systems** (Pi OS Lite / framebuffer or Wayland):
  `autovideosink` does not always pick the right sink; force one with
  `RPIPLAY_VIDEOSINK=kmssink` (framebuffer console) or `RPIPLAY_VIDEOSINK=waylandsink`.

Several parts of the GStreamer pipeline can be tuned with environment
variables, mainly for debugging:

| Variable | Meaning |
|---|---|
| `RPIPLAY_VDECODER` | Force the H.264 decoder element (e.g. `avdec_h264` for software decoding, or to work around a broken hardware decoder). Default: `v4l2h264dec` if available, else `decodebin`. |
| `RPIPLAY_VIDEOSINK` | Force the video sink element (e.g. `kmssink`, `waylandsink`, `glimagesink`). Default: `autovideosink`. |
| `RPIPLAY_AUDIOSINK` | Force the audio sink element (e.g. `alsasink`, `pipewiresink`). Default: `autoaudiosink`. |
| `RPIPLAY_BT709` | Set to `1` to force BT.709 colorimetry; needed with `v4l2h264dec` when mirrored colours look washed out. |

# Audio output on external DACs (e.g. HiFiBerry)

The Raspberry Pi OpenMAX audio renderer can only address the firmware's built-in
outputs ("local" and "hdmi"); external I2S sound cards are not reachable through it.
To play on an external DAC such as the [HiFiBerry DAC+](https://www.hifiberry.com/dacs/)
family, use the ALSA audio renderer:

```bash
rpiplay -ar alsa -a analog
```

With `-a analog`, the ALSA renderer automatically uses a HiFiBerry card if one is
detected, and falls back to the system default device otherwise. You can also point
it at any ALSA device explicitly:

```bash
rpiplay -ar alsa -a sysdefault:CARD=sndrpihifiberrydacplus
```

Run `aplay -l` to list the card names on your system. Volume changes from the phone
are mapped onto the DAC's hardware mixer where one is available (e.g. DAC+/Pro with
the PCM5122 chip); on boards without a hardware mixer (e.g. DAC+ Light) volume
control is ignored with a warning.

The DAC itself is set up on the OS side, independently of RPiPlay. Enable the
matching device tree overlay in `/boot/config.txt` (see the
[HiFiBerry configuration guide](https://www.hifiberry.com/docs/software/configuring-linux-3-18-x/)):

| Board | dtoverlay |
|---|---|
| DAC (Pi 1), DAC+ Light, DAC Zero, MiniAmp, DAC+ DSP, DAC+ RTC | `hifiberry-dac` |
| DAC+ Standard, DAC+ Pro, DAC2 Pro, Amp2, Amp4 | `hifiberry-dacplus` (kernels >= 6.1.77: `hifiberry-dacplus-std` / `-pro`) |
| DAC2 HD | `hifiberry-dacplushd` |
| DAC+ ADC / DAC+ ADC Pro | `hifiberry-dacplusadc` / `hifiberry-dacplusadcpro` |
| Digi+ / Digi2 Pro (S/PDIF) | `hifiberry-digi` / `hifiberry-digi-pro` |

After rebooting, verify the card shows up with `aplay -l`. If you want *all*
system audio (not just RPiPlay) to use the DAC, also set the ALSA default card as
described in the guide.

Note: RPiPlay receives audio as part of the mirroring stream (AAC-ELD). Audio-only
AirPlay streaming (e.g. AirTunes/ALAC from music apps) is not supported, so use a
sender that mirrors the screen (or its audio) to the RPiPlay device.

## Alternative: Bluetooth A2DP (from any phone)

AirPlay mirroring requires a sender app on Android. If you just want music from a
phone on the DAC, the Pi can also act as a Bluetooth speaker — that needs no
sender app at all and works with every phone natively.

Two software options:

**BlueALSA (recommended — lightweight, reliable on the Pi Zero):**

```bash
sudo apt-get install -y bluez-alsa-utils
sudo tee /etc/systemd/system/bluealsa.service.d/a2dp.conf >/dev/null <<'EOF'
[Service]
ExecStart=
ExecStart=/usr/bin/bluealsa -p a2dp-sink
EOF
sudo systemctl daemon-reload
sudo systemctl enable --now bluealsa
# play received audio through the DAC:
sudo systemctl enable --now bluealsa-aplay
```

`bluealsa-aplay` routes the decoded Bluetooth audio to the ALSA default
device — with `pipewire-alsa` installed that is PipeWire, so Bluetooth and
AirPlay mix on the same DAC.

**PipeWire Bluetooth (when the image's bluez5 SPA plugin is intact):**
install `libspa-0.2-bluetooth`, set
`bluez5.roles = [ a2dp_sink ]` (and `enable-sbc-xq`, `enable-hw-volume`) in
the `monitor.bluez.properties` section of the WirePlumber config, and
restart WirePlumber. Note: on some images this monitor does not register
A2DP endpoints (verify with `journalctl -u bluetooth | grep -i endpoint`);
BlueALSA is the dependable fallback.

Both approaches need the Pi made visible once (see `bt-agent`/bluetoothctl
setup in the deploy kit, `deploy/README.md`) and the phone paired + trusted.

# Global installation

After building, to install the executable on the system permanently (so it can be run from anywhere), simply run the following command:
```bash
sudo make install
```

A systemd service template is installed to `lib/systemd/system/rpiplay.service`,
so RPiPlay can also be started at boot:
```bash
sudo systemctl enable --now rpiplay
```
The service runs `rpiplay` without arguments by default. To change the command
line, set the CMake cache variable when building (e.g.
`cmake .. -DRPIPLAY_SERVICE_ARGS="-vr gstreamer -ar alsa -a analog"` and
reinstall), or override it on the target system with a drop-in:
`systemctl edit rpiplay`. When running as a system service, consider adding a
`User=` line for an unprivileged user with audio access.

# Usage

Start the rpiplay executable and an AirPlay mirror target device will appear in the network.
At the moment, these options are implemented:

**-n name**: Specify the network name of the AirPlay server.

**-b (on|auto|off)**: Show black background always, only during active connection, or never.

**-r (90|180|270)**: Specify image rotation in multiples of 90 degrees.

**-f (horiz|vert|both)**: Specify image flipping.

**-l**: Enables low-latency mode. Low-latency mode reduces latency by effectively rendering audio and video frames as soon as they are received, ignoring the associated timestamps. As a side effect, playback will be choppy and audio-video sync will be noticably off.

**-a (hdmi|analog|off)**: Set audio output device. The alsa renderer additionally accepts an explicit ALSA device name (e.g. `sysdefault:CARD=sndrpihifiberrydacplus`) and automatically plays through a HiFiBerry DAC for `analog` if one is present (see the section on external DACs below).

**-vr renderer**: Select a video renderer to use (rpi, gstreamer, or dummy)

**-ar renderer**: Select an audio renderer to use (rpi, alsa, gstreamer, or dummy)

**-d**: Enables debug logging. Will lead to choppy playback due to heavy console output.

**-v/-h**: Displays short help and version information.


# Disclaimer

All the resources in this repository are written using only freely available information from the internet. The code and related resources are meant for educational purposes only. It is the responsibility of the user to make sure all local laws are adhered to.

This project makes use of a third-party GPL library for handling FairPlay. The legal status of that library is unclear. Should you be a representative of Apple and have any objections against the legality of the library and its use in this project, please contact me and I'll take the appropriate steps.

Given the large number of third-party AirPlay receivers (mostly closed-source) available for purchase, it is my understanding that an open source implementation of the same functionality wouldn't violate any of Apple's rights either.


# Authors

The code in this repository accumulated from various sources over time. Here is my attempt at listing the various authors and the components they created:

* **dsafa22**: Created an [AirPlay 2 mirroring server](https://github.com/dsafa22/AirplayServer)(seems gone now) for Android based on ShairPlay. This project is basically a port of dsafa22's code to the Raspberry Pi, utilizing OpenMAX and OpenSSL for better performance on the Pi. All code in `lib/` concerning mirroring is dsafa22's work. License: GNU LGPLv2.1+
* **Juho Vähä-Herttua** and contributors: Created an AirPlay audio server called [ShairPlay](https://github.com/juhovh/shairplay), including support for Fairplay based on PlayFair. Most of the code in `lib/` originally stems from this project. License: GNU LGPLv2.1+
* **EstebanKubata**: Created a FairPlay library called [PlayFair](https://github.com/EstebanKubata/playfair). Located in the `lib/playfair` folder. License: GNU GPL
* **Joyent, Inc and contributors**: Created an http library called [llhttp](https://github.com/nodejs/llhttp). Located at `lib/llhttp/`. License: MIT
* **Team XBMC**: Managed to show a black background for OpenMAX video rendering. This code is used in the video renderer. License: GNU GPL
* **Alex Izvorski and contributors**: Wrote [h264bitstream](https://github.com/aizvorski/h264bitstream), a library for manipulation h264 streams. Used for reducing delay in the Raspberry Pi video pipeline. Located in the `renderers/h264-bitstream` folder. License: GNU LGPLv2.1


# Contributing

I'm afraid I won't have time to regularly maintain this project. Instead, I'm hoping this project can be improved in a community effort. I'll fix and add as much as I need for personal use, and I count on you to do the same!

Your contributions are more than welcome!


# Todo

* Bug: Sometimes cannot be stopped?

# Changelog

### Version 1.3 (unreleased)

* New `alsa` audio renderer (fdk-aac decoding, ALSA output) — required for external
  I2S DACs such as the HiFiBerry family, which the OpenMAX renderer cannot reach.
  Auto-selects a HiFiBerry card with `-a analog`; `-a` also accepts explicit ALSA
  device names; phone volume is mapped to the DAC hardware mixer.
* GStreamer video renderer: automatically uses the Broadcom hardware H.264 decoder
  (`v4l2h264dec`) when available (Pi 3/4/Zero 2 W), falls back to `decodebin`
  (software decoding, also the correct path on Pi 5). Optional BT.709 colorimetry
  fix for the v4l2 decoder. Decoder and sinks can be overridden with the
  `RPIPLAY_VDECODER`, `RPIPLAY_VIDEOSINK`, `RPIPLAY_AUDIOSINK` and `RPIPLAY_BT709`
  environment variables; the selected decoder is logged at startup.
* Audio quality fixes in the OpenMAX audio renderer: broken chunked-output copy
  (first chunk was duplicated when the OMX input buffer could not hold a full
  frame), fdk-aac error concealment for lost packets, preallocated decode buffer,
  forced stereo output, fixed NULL dereference in audio renderer init failure path.
* GStreamer renderers: required-plugin check was skipped in release builds
  (assert compiled out with NDEBUG); it now fails initialization with a clear
  message instead.

### Version 1.2

* Blank screen after connection stopped

### Version 1.1

* Now audio and video work on Raspberry Pi Zero. I don't know what exactly did the trick, but static compilation seems to have helped.
* Smoother video due to clock syncing
* Correct lip-sync due to clock syncing
* Lower latency due to injecting max_dec_frame_buffering into SPS NAL 
* Disabled debug logging by default
* Added command line flag for debug logging
* Added command line flag for unsynchronized low-latency mode
* Bug fixes


# AirPlay protocol versions

For multiple reasons, it's very difficult to clearly define the protocol names and versions of the components that make up the AirPlay streaming system. In fact, it seems like the AirPlay version number used for marketing differs from that used in the actual implementation. In order to tidy up this whole mess a bit, I did a little research that I'd like to summarize here:


The very origin of the AirPlay protocol suite was launched as AirTunes sometime around 2004. It allowed to stream audio from iTunes to an AirPort Express station. Internally, the name of the protocol that was used was RAOP, or Remote Audio Output Protocol. It seems already back then, the protocol involved AES encryption. A public key was needed for encrypting the audio sent to an AirPort Express, and the private key was needed for receiving the protocol (ie used in the AirPort Express to decrypt the stream). Already in 2004, the public key was reverse-engineered, so that [third-party sender applications](http://nanocr.eu/2004/08/11/reversing-airtunes/) were developed.


Some time [around 2008](https://weblog.rogueamoeba.com/2008/01/10/a-tour-of-airfoil-3/), the protocol was revised and named AirTunes 2. It seems the changes primarily concerned timing. By 2009, the new protocol was [reverse-engineered and documented](https://git.zx2c4.com/Airtunes2/about/).


When the Apple TV 2nd generation was introduced in 2010, it received support for the AirTunes protocol. However, because this device allowed playback of visual content, the protocol was extended and renamed AirPlay. It was now possible to stream photo slideshows and videos. Shortly after the release of the Apple TV 2nd generation, AirPlay support for iOS was included in the iOS 4.2 update. It seems like at that point, the audio stream was still actually using the same AirTunes 2 protocol as described above. The video and photo streams were added as a whole new protocol based on HTTP, pretty much independent from the audio stream. Soon, the first curious developers began to [investigate how it worked](https://web.archive.org/web/20101211213705/http://www.tuaw.com/2010/12/08/dear-aunt-tuaw-can-i-airplay-to-my-mac/). Their conclusion was that visual content is streamed unencrypted.


In April 2011, a talented hacker [extracted the AirPlay private key](http://www.macrumors.com/2011/04/11/apple-airplay-private-key-exposed-opening-door-to-airport-express-emulators/) from an AirPort Express. This meant that finally, third-party developers were able to also build AirPlay reveiver (server) programs.


For iOS 5, released in 2011, Apple added a new protocol to the AirPlay suite: AirPlay mirroring. [Initial investigators](https://www.aorensoftware.com/blog/2011/08/20/exploring-airplay-mirroring-internals/) found this new protocol used encryption in order to protect the transferred video data.


By 2012, most of AirPlay's protocols had been reverse-engineered and [documented](https://nto.github.io/AirPlay.html). At this point, audio still used the AirTunes 2 protocol from around 2008, video, photos and mirroring still used their respective protocols in an unmodified form, so you could still speak of AirPlay 1 (building upon AirTunes 2). The Airplay server running on the Apple TV reported as version 130. The setup of AirPlay mirroring used the xml format, in particular a stream.xml file.
Additionally, it seems like the actual audio data is using the ALAC codec for audio-only (AirTunes 2) streaming and AAC for mirror audio. At least these different formats were used in [later iOS versions](https://github.com/espes/Slave-in-the-Magic-Mirror/issues/12#issuecomment-372380451).


Sometime before iOS 9, the protocol for mirroring was slightly modified: Instead of the "stream.xml" API endpoint, the same information could also be querried in binary plist form, just by changing the API endpoint to "stream", without any extension. I wasn't able to figure out which of these was actually used by what specific client / server versions.


For iOS 9, Apple made [considerable changes](https://9to5mac.com/2015/09/11/apple-ios-9-airplay-improvements-screen-mirroring/) to the AirPlay protocol in 2015, including audio and mirroring. Apparently, the audio protocol was only slightly modified, and a [minor change](https://github.com/juhovh/shairplay/issues/43) restored compatibility. For mirroring, an [additional pairing phase](https://github.com/juhovh/shairplay/issues/43#issuecomment-142115959) was added to the connection establishment procedure, consisting of pair-setup and pair-verify calls. Seemingly, these were added in order to simplify usage with devices that are connected frequently. Pair-setup is used only the first time an iOS device connects to an AirPlay receiver. The generated cryptographic binding can be used for pair-verify in later sessions. Additionally, the stream / stream.xml endpoint was replaced with the info endpoint (only available as binary plist AFAICT).
As of iOS 12, the protocol introduced with iOS 9 was still supported with only slight modifications, albeit as a legacy mode. While iOS 9 used two SETUP calls (one for general connection and mirroring video, and one for audio), iOS 12 legacy mode uses 3 SETUP calls (one for general connection (timing and events), one for mirroring video, one for audio).


The release of tvOS 10.2 broke many third-party AirPlay sender (client) programs in 2017. The reason was that it was now mandatory to perform device verification via a pin in order to stream content to an Apple TV. The functionality had been in the protocol before, but was not mandatory. Some discussion about the new scheme can be found [here](https://github.com/postlund/pyatv/issues/79). A full specification of the pairing and authentication protocol was made available on [GitHub](https://htmlpreview.github.io/?https://github.com/philippe44/RAOP-Player/blob/master/doc/auth_protocol.html). At that point, tvOS 10.2 reported as AirTunes/320.20.


In tvOS 11, the reported server version was [increased to 350.92.4](https://github.com/ejurgensen/forked-daapd/issues/377#issuecomment-309213273).


iOS 11.4 added AirPlay 2 in 2018. Although extensively covered by the media, it's not entirely clear what changes specifically Apple has made protocol-wise.


From captures of the traffic between an iOS device running iOS 12.2 and an AppleTV running tvOS 12.2.1, one can see that the communication on the main mirroring HTTP connection is encrypted after the initial handshake.
This could theoretically be part of the new AirPlay 2 protocol. The AppleTV running tvOS 12.2.1 identifies as AirTunes/380.20.1.
When connecting from the same iOS device to an AppleTV 3rd generation (reporting as AirTunes/220.68), the communication is still visible in plain. From the log messages that the iOS device produces when connected to an AppleTV 3rd generation, it becomes apparent that the iOS device is treating this plain protocol as the legacy protocol (as originally introduced with iOS 9). Further research showed that at the moment, all available third-party AirPlay mirroring receivers (servers) are using this legacy protocol, including the open source implementation of dsafa22, which is the base for RPiPlay. Given Apple considers this a legacy protocol, it can be expected to be removed entirely in the future. This means that all third-party AirPlay receivers will have to be updated to the new (fully encrypted) protocol at some point.

More specifically, the encryption starts after the pair-verify handshake completed, so the fp-setup handshake is already happening encrypted. Judging from the encryption scheme for AirPlay video (aka HLS Relay), likely two AES GCM 128 ciphers are used on the socket communication (one for sending, one for receiving). However, I have no idea how the keys are derived from the handshake data.
