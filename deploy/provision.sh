#!/bin/bash
# RPiPlay + InnoMaker DAC Mini HiHat (PCM5122) provisioning for
# Raspberry Pi OS (64-bit, Trixie or Bookworm). Run as user 'pi' ON the Pi:
#
#   cd <repo>/deploy && ./provision.sh
#
# The script uses sudo for system changes (one password prompt up front).
set -e

if [ "$(id -u)" -eq 0 ]; then
    echo "Run this as the regular user (pi), not as root."; exit 1
fi
sudo -v   # cache credentials once

REPO="$(cd "$(dirname "$0")/.." && pwd)"
KVER="$(uname -r)"
echo "== target kernel: $KVER =="

echo "== [1/6] Installing system packages =="
sudo apt-get update
sudo apt-get install -y build-essential cmake pkg-config git rsync \
    libssl-dev libplist-dev libasound2-dev libavahi-compat-libdnssd-dev \
    libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
    gstreamer1.0-libav gstreamer1.0-plugins-bad gstreamer1.0-plugins-good \
    gstreamer1.0-tools pipewire-alsa bluez-alsa-utils bluez-tools
# Kernel headers may already ship with some images (e.g. /usr/src prepopulated);
# only install the package if no matching build dir exists for the running kernel.
if [ ! -d "/lib/modules/$KVER/build" ]; then
    sudo apt-get install -y raspberrypi-kernel-headers
fi
[ -d "/lib/modules/$KVER/build" ] || { echo "ERROR: no kernel build dir for $KVER"; exit 1; }

echo "== [2/6] Deploying rpiplay source and building =="
rsync -a --delete --exclude build --exclude .git --exclude deploy "$REPO/" "$HOME/RPiPlay/"
mkdir -p "$HOME/RPiPlay/build"
cmake -S "$HOME/RPiPlay" -B "$HOME/RPiPlay/build" -DCMAKE_BUILD_TYPE=Release
make -C "$HOME/RPiPlay/build" -j"$(nproc)"

echo "== [3/6] Boot configuration (InnoMaker DAC via allo-boss overlay) =="
CFG=/boot/firmware/config.txt
[ -f "$CFG" ] || CFG=/boot/config.txt
if ! grep -q "allo-boss-dac-pcm512x-audio" "$CFG"; then
    sudo cp "$CFG" "$CFG.bak"
    sudo tee -a "$CFG" >/dev/null <<'EOF'

# InnoMaker DAC Mini HiHat (PCM5122) via the Allo Boss DAC overlay:
# the allo-boss machine driver runs the PCM5122 in codec-master mode
# (the HAT's onboard oscillators) and uses dummy regulators, which
# avoids the 6.12+ runtime-PM -22 open failure of the hifiberry-dacplus
# overlays. The resulting card is named "BossDAC".
dtparam=i2c_arm=on
dtoverlay=allo-boss-dac-pcm512x-audio
EOF
fi
grep -n "allo-boss\|hifiberry" "$CFG"

echo "== [4/6] Patched pcm512x kernel module (works around 6.18 runtime-PM -22) =="
MODDIR="/lib/modules/$KVER/kernel/sound/soc/codecs"
if [ -f "$REPO/deploy/modules/snd-soc-pcm512x-$KVER.ko" ]; then
    echo "  using prebuilt module for $KVER"
    sudo install -m 644 "$REPO/deploy/modules/snd-soc-pcm512x-$KVER.ko" \
        "$MODDIR/snd-soc-pcm512x.ko"
else
    echo "  building patched module for $KVER"
    FIX="$HOME/pcm512x-fix"
    mkdir -p "$FIX"
    cp "$REPO/deploy/pcm512x-fix/pcm512x.c" "$REPO/deploy/pcm512x-fix/pcm512x.h" \
       "$REPO/deploy/pcm512x-fix/Makefile" "$FIX/"
    make -C "$FIX" -j"$(nproc)"
    sudo cp "$MODDIR/snd-soc-pcm512x.ko" "$MODDIR/snd-soc-pcm512x.ko.orig" 2>/dev/null || true
    sudo rm -f "$MODDIR/snd-soc-pcm512x.ko.xz"
    sudo install -m 644 "$FIX/snd-soc-pcm512x.ko" "$MODDIR/snd-soc-pcm512x.ko"
fi
sudo depmod -a "$KVER"

echo "== [5/6] Installing systemd services =="
# The units are generated with the actual user/home/uid, so provisioning
# works on images where the account is not named "pi".
TGT_USER="$(id -un)"
TGT_HOME="$HOME"
TGT_UID="$(id -u)"

sudo install -m 644 "$REPO/deploy/etc-default-rpiplay" /etc/default/rpiplay
sudo install -m 755 "$REPO/deploy/bt-agent.sh" /usr/local/sbin/bt-agent.sh
sudo install -m 644 "$REPO/deploy/systemd/bt-agent.service" \
    /etc/systemd/system/bt-agent.service

sudo tee /etc/systemd/system/rpiplay.service >/dev/null <<EOF
[Unit]
Description=RPiPlay AirPlay mirroring server
After=network-online.target avahi-daemon.service
Wants=network-online.target

[Service]
User=${TGT_USER}
WorkingDirectory=${TGT_HOME}/RPiPlay/build
EnvironmentFile=/etc/default/rpiplay
Environment=XDG_RUNTIME_DIR=/run/user/${TGT_UID}
ExecStart=${TGT_HOME}/RPiPlay/build/rpiplay \$RPIPLAY_ARGS
Restart=on-failure
RestartSec=3

[Install]
WantedBy=multi-user.target
EOF

# BlueALSA: A2DP sink endpoints + player routing BT audio into PipeWire
sudo tee /etc/systemd/system/bluealsa-aplay.service >/dev/null <<EOF
[Unit]
Description=BlueALSA A2DP player (BT audio to PipeWire)
After=bluealsa.service bluetooth.service
Requires=bluealsa.service

[Service]
Environment=XDG_RUNTIME_DIR=/run/user/${TGT_UID}
ExecStart=/usr/bin/bluealsa-aplay
Restart=always
RestartSec=3

[Install]
WantedBy=multi-user.target
EOF

sudo mkdir -p /etc/systemd/system/bluealsa.service.d
sudo install -m 644 "$REPO/deploy/systemd/bluealsa-a2dp.conf" \
    /etc/systemd/system/bluealsa.service.d/a2dp.conf
sudo systemctl daemon-reload
sudo systemctl enable --now rpiplay.service bt-agent.service \
    bluealsa.service bluealsa-aplay.service

echo "== [6/6] Enabling user lingering (PipeWire at boot, headless) =="
sudo loginctl enable-linger "$USER"

cat <<'EOF'

Done. Reboot to activate the DAC overlay, then:

  1. Check the card:        aplay -l   (expect sndrpihifiberry)
  2. Make the Pi visible:   sudo sh -c 'printf "discoverable on\nquit\n" | bluetoothctl'
  3. Pair the phone (Settings > Bluetooth > raspberrypi), then:
       sudo sh -c 'printf "trust <phone-mac>\nquit\n" | bluetoothctl'
  4. Play music (Bluetooth output "raspberrypi", or AirPlay Screen Mirroring
     to "RPiPlay" from iOS).

rpiplay args live in /etc/default/rpiplay. The patched pcm512x module logs
"regulator_bulk_enable failed"/"regcache_sync failed" at boot - these are
expected (see deploy/README.md) and harmless.
EOF
