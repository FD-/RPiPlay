# Introduction

An open-source implementation of an AirPlay mirroring server for the Raspberry Pi.
The goal is to make it run smoothly even on a Raspberry Pi Zero.


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
mkdir build
cd build
cmake ..
make
```

GCC 5 or later is required.

# Building on desktop Linux:

For building on Ubuntu 18.04 or 20.04, follow these steps:
```bash
sudo apt-get install cmake libavahi-compat-libdnssd-dev libplist-dev libssl-dev \
    libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev gstreamer1.0-libav \
    gstreamer1.0-vaapi gstreamer1.0-plugins-bad
mkdir build
cd build
cmake ..
make
```

Note: The -b, -r, -l, and -a options are not supported with the gstreamer renderer.

# Building in docker for LibreElec for Rasberry Pi

Since it is not possible to install the build tools in LibreElec, the build can be make by launching the `build_with_docker.sh` script in the `docker-builder` folder.
The LibreElec docker extension should be installed prior to starting the build.

# Usage

Start the rpiplay executable and an AirPlay mirror target device will appear in the network.
At the moment, these options are implemented:

**-n name**: Specify the network name of the AirPlay server.

**-b (on|auto|off)**: Show black background always, only during active connection, or never.

**-r (90|180|270)**: Specify image rotation in multiples of 90 degrees.

**-f (horiz|vert|both)**: Specify image flipping.

**-l**: Enables low-latency mode. Low-latency mode reduces latency by effectively rendering audio and video frames as soon as they are received, ignoring the associated timestamps. As a side effect, playback will be choppy and audio-video sync will be noticably off.

**-a (hdmi|analog|off)**: Set audio output device

**-vr renderer**: Select a video renderer to use (rpi, gstreamer, or dummy)

**-ar renderer**: Select an audio renderer to use (rpi, gstreamer, or dummy)

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
* **Joyent, Inc and contributors**: Created an http library called [http-parser](https://github.com/nodejs/http-parser). Located at `lib/http_parser.(c|h)`. License: MIT
* **Google, Inc and contributors**: Created an implementation of curve 25519 called [curve25519-donna](https://github.com/agl/curve25519-donna). Located in the `lib/curve25519` folder. License: 3-Clause BSD
* **Team XBMC**: Managed to show a black background for OpenMAX video rendering. This code is used in the video renderer. License: GNU GPL
* **Orson Peters and contributors**: An implementation of [Ed25519](https://github.com/orlp/ed25519) signatures. Located in `lib/ed25519`, License: ZLIB; Depends on LibTomCrypt, License: Public Domain
* **Alex Izvorski and contributors**: Wrote [h264bitstream](https://github.com/aizvorski/h264bitstream), a library for manipulation h264 streams. Used for reducing delay in the Raspberry Pi video pipeline. Located in the `renderers/h264-bitstream` folder. License: GNU LGPLv2.1


# Contributing

I'm afraid I won't have time to regularly maintain this project. Instead, I'm hoping this project can be improved in a community effort. I'll fix and add as much as I need for personal use, and I count on you to do the same!

Your contributions are more than welcome!


# Todo

* Use OpenSSL for the elliptic curve crypto?
* Bug: Sometimes cannot be stopped?

# Changelog

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
