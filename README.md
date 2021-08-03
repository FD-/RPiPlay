This project is an early stage prototype of unix AirPlay server.
Work is based on https://github.com/FD-/RPiPlay.
Tested on Ubuntu 19.10 desktop.
5G Wifi connection is the must.

Features:
1. Based on Gstreamer.
2. Video and audio are supported out of the box.
3. Gstreamer decoding is plugin agnostic. Uses accelerated decoders if
available. VAAPI is preferable. (but don't use VAAPI with nVidia)
4. Automatic screen orientation.

Getting it:  (after sudo apt-get-install git)
git clone https://github.com/FDH2/UxPlay.git .  This is a pull request to the
original site https://github.com/antimof/UxPlay.git ; it may or may not ever
get "pulled" into the original site, as the antimof site may no longer be active.

Building this version

1. sudo apt-get install cmake
2. sudo apt-get install libssl-dev libavahi-compat-libdnssd-dev
libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev gstreamer1.0-libav gstreamer1.0-plugins-bad  libplist-dev
3. sudo apt-get install gstreamer1.0-vaapi (For Intel graphics, but not nVidia graphics)
4. sudo apt-get install libx11-dev  (for the X_display name fix for screen-sharing with e.g.,  ZOOM)
4. mkdir build
5. cd build
6. cmake ..      (or "cmake -DZOOMFIX=ON .." to get the screen-sharing fix)
7. make
8. sudo make install

Note libplist-dev and (for ZOOMFIX) libx11-dev are new dependencies.

Options:
**-n server_name **;  server_name will be the name that appears offering
AirPlay services to your iPad, iPhone etc.
**NEW**: this will also be the name on the mirror window, if "ZOOMFIX" is
applied when compiling uxplay.

**-s wxh** (e.g. -s 1920x1080 , which is the default ) sets the display resolution (width and height,
   in pixels).   (This may be a
   request made to the AirPlay client, and perhaps will not
   be the final resolution you get).

**-p**    allows you to select the network ports used by UxPlay (these need
   to be opened if the server is behind a firewall)   By itself, -p sets
   "legacy" ports TCP 7100, 7000, 7001, UDP 6000, 6001, 7011.   -p n (e.g. -p
   35000)  sets TCP and UDP ports n, n+1, n+2.  Ports must be in thye range
   [1024-65535].

If the -p option is not used, the ports are chosen dynamically (randomly),
which will not work if a firewall is running.

**-r**  generates a random MAC address to use instead of the true hardware MAC
   number of the computer's network card.   (Different server_name,  MAC
   addresses,  and network ports are needed for each running uxplay  if you
   attempt to  run two instances of uxplay on thye same computer.)

**-f {H|V|R|L|I} **  implements "videoflip"  image transforms: H = horizontal flip
(right-left mirror); V = vertical flip (up-down mirror); R = 90 deg. clockwise
rotation; L = 90 deg counter-clockwise rotation; I = 180 deg. rotation or inversion.

**-a** disable audio, leaving only the video playing.


New features available: (v 1.3 2021-08)

1. Updates of the RAOP (AirPlay protocol, not AirPlay 2)  collection of codes  maintained
at  https://github.com/FD-/RPiPlay.git so it is current as of 2021-08-01,
adding all changes since the original release of UxPlay by antimof.
This involved crypto updates, replacement
of the included plist library by the system-installed version, and  a change to
lib llhttp for html parsing. 

2. added The -p, -s, -r and -f options.

3. If "cmake -DZOOMFIX=ON .."  is run before compiling,
the mirrored window is now visible to screen-sharing applications such as
Zoom.     You can tell if the "ZOOMFIX"
is working by examining the title bar on the mirror window:
it will be "uxplay" without the fix, and be the AirPlay server_name, which
is "UxPlay" (note capitals) by default, and can be changed with the -n option.
To compile with ZOOMFIX=ON, the X11 development libraries must be installed.
(ZOOMFIX will not be needed once  gstreamer-1.20 is available, since this
update will make the gstreamer mirror window visible to screen-sharers.)
(Thanks to David Ventura  https://github.com/DavidVentura/UxPlay ).

4. The avahi_compat nag warning on startup is suppressed.

5.   In principle, multiple instances of uxplay can be run simultaneously
using the -r "random MAC address" option to give each a different
(randomly-chosen "local") MAC address.
If the -p option is used, they also need separate network port choices.
(However, there may be a large latency, and running two instances of uxplay
simultaneously may not be very useful.)

6.  Without the -p [n] option,  uxplay makes a random dynamic assignment of
network ports. This will not work if most ports are closed by a firewall.
With e.g., -p 45000   you should open both TCP and UDP on
ports 45000, 45001, 45002.   Minimum allowed port is 1024, maximum is 65535.
The option "-p " with no argument uses a "legacy" set of ports TCP 7100,
7000, 7001, and UDP  7011, 6000, 6001.

7.  The default resolution setting is 1920x1080 width x height pixels.
To change this, use "-s wxh" (or wXh) where w and h are positive  decimals
with 4 or less digits.   It seems that the width and height may be negotiated
with the AirPlay client, so this may not be the actual screen geometry that
displays.
