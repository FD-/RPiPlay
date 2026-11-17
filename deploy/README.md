# Deploy — Bluetooth speaker + AirPlay receiver on a Raspberry Pi

Turn a Raspberry Pi (Zero W/2W or newer) into a **Bluetooth A2DP speaker**
and **AirPlay mirroring receiver**, with the audio output on an I2S DAC HAT
(InnoMaker DAC Mini HiHat / PCM5122 — HiFiBerry DAC+ compatible).

Everything was verified end-to-end on a Raspberry Pi Zero W v1.1 running
Raspberry Pi OS Lite Trixie (32-bit armhf, kernel 6.18.50+rpt-rpi-v6), with music
streamed from Android (Bluetooth) and iPhone (AirPlay) through PipeWire onto
the DAC.

---

# Quick start

1. Flash **Raspberry Pi OS Lite** on an SD card with ≥8GB using the latest version of [`rpi-imager`](https://www.raspberrypi.com/software/) setting up username password, wifi and enable SSH, wait for boot to finish.
2. Clone this repo [git@text70/RPriPlay](https://github.com/text70/RPiPlay.git). Then run the following in order to copy the Pi using `rsync`, SSH in and run the provisioner script:

```bash
cd ~/RPiPlay && rsync -a --exclude build --exclude .git ./ pi@<pi-address>:RPiPlay/
ssh pi@<pi-address>
cd ~/RPiPlay/deploy && ./provision.sh
sudo reboot
```
>[!TIP]
>The `provision.sh` process can take thirty minutes to an hour to patch and compile depending on your Pi version. 

3. After the reboot, pair your phone:
   - **Phone → Settings → Bluetooth → "raspberrypi" → Pair** (no PIN needed)
   - On the Pi, mark it trusted so it reconnects at boot:

```bash
bluetoothctl devices Paired          # note the phone MAC address
bluetoothctl trust <MAC>
```

4. Play music on the phone and select **raspberrypi** as the output.
   For AirPlay (iPhone): Screen Mirroring → **RPiPlay**.

That's it. Everything below is the manual equivalent and the reasoning.

---

# Manual step-by-step setup

Run all commands on the Pi as user `pi` (sudo needed in several steps).

## Step 1 — System packages

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake pkg-config git rsync \
    libssl-dev libplist-dev libasound2-dev libavahi-compat-libdnssd-dev \
    libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
    gstreamer1.0-libav gstreamer1.0-plugins-bad gstreamer1.0-plugins-good \
    gstreamer1.0-tools pipewire-alsa bluez-alsa-utils bluez-tools
```

* `pipewire-alsa` — routes the ALSA `default` device into PipeWire (this is
  how Bluetooth and AirPlay audio reach the DAC)
* `bluez-alsa-utils` — the BlueALSA A2DP sink (`bluealsa` + `bluealsa-aplay`)
* `bluez-tools` — the `bt-agent` pairing daemon used below
* Kernel headers: needed later for the patched DAC driver. Some images ship
  them already (`/lib/modules/$(uname -r)/build` exists); otherwise:

```bash
sudo apt-get install -y raspberrypi-kernel-headers
```

## Step 2 — Enable the DAC HAT

Add to `/boot/firmware/config.txt` (older images: `/boot/config.txt`):

```
dtparam=i2c_arm=on
dtoverlay=allo-boss-dac-pcm512x-audio
```

The **allo-boss-dac** overlay drives the PCM5122 chip (also used by the
InnoMaker board) in codec-master mode using the HAT's onboard oscillators,
and avoids a runtime-PM bug that breaks the `hifiberry-dacplus*` overlays on
kernels ≥ 6.12 (details at the bottom). The card registers as `BossDAC`.

**Keep the HDMI cable unplugged** on the Zero: the connector shell sits next
to the header and can short HAT pins.

## Step 3 — Build RPiPlay

```bash
git clone https://github.com/FD-/RPiPlay.git   # or copy this repo
cd RPiPlay
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

## Step 4 — Patched PCM512x kernel module

Raspberry Pi OS kernels ≥ 6.12 fail to open PCM512x sound cards with
`ASoC error (-22)` (runtime-PM bug, details at the bottom). The fixes live in
`deploy/pcm512x-fix/`:

```bash
cp -r RPiPlay/deploy/pcm512x-fix ~/pcm512x-fix && cd ~/pcm512x-fix
python3 patch_pcm512x.py pcm512x.c        # tolerate resume failures
python3 patch_pcm512x_retry.py pcm512x.c  # retry boot probe ~60s
python3 patch_pcm512x_dacref.py pcm512x.c # fix silent DAC clock source
make
sudo cp /lib/modules/$(uname -r)/kernel/sound/soc/codecs/snd-soc-pcm512x.ko* \
    /tmp/                                 # keep the originals
sudo rm -f /lib/modules/$(uname -r)/kernel/sound/soc/codecs/snd-soc-pcm512x.ko.xz
sudo install -m 644 snd-soc-pcm512x.ko \
    /lib/modules/$(uname -r)/kernel/sound/soc/codecs/snd-soc-pcm512x.ko
sudo depmod -a
```

This module also restores the **hardware volume mixer** of the PCM5122, so
your phone's volume keys work.

## Step 5 — WirePlumber A2DP settings

```bash
sudo mkdir -p /etc/wireplumber/wireplumber.conf.d
sudo tee /etc/wireplumber/wireplumber.conf.d/51-bluez-a2dp.conf >/dev/null <<'EOF'
monitor.bluez.properties = {
  bluez5.enable-sbc-xq = true
  bluez5.enable-msbc = false
  bluez5.enable-hw-volume = true
  bluez5.roles = [ a2dp_sink ]
}
EOF
```

## Step 6 — systemd services

The units below are generated for **your** user/home/uid, so they work on
any Raspberry Pi OS image regardless of the account name.

**rpiplay** (AirPlay receiver):

```bash
sudo tee /etc/default/rpiplay >/dev/null <<'EOF'
RPIPLAY_ARGS="-vr dummy -ar alsa -a pipewire"
EOF
sudo tee /etc/systemd/system/rpiplay.service >/dev/null <<EOF
[Unit]
Description=RPiPlay AirPlay mirroring server
After=network-online.target avahi-daemon.service
Wants=network-online.target

[Service]
User=$(id -un)
WorkingDirectory=$HOME/RPiPlay/build
EnvironmentFile=/etc/default/rpiplay
Environment=XDG_RUNTIME_DIR=/run/user/$(id -u)
ExecStart=$HOME/RPiPlay/build/rpiplay \$RPIPLAY_ARGS
Restart=on-failure
RestartSec=3

[Install]
WantedBy=multi-user.target
EOF
```

**Bluetooth pairing agent** (auto-pairing, adapter power/discoverable —
install `deploy/bt-agent.sh` first, it also clears the rfkill soft-block
that some images ship with):

```bash
sudo install -m 755 RPiPlay/deploy/bt-agent.sh /usr/local/sbin/bt-agent.sh
sudo tee /etc/systemd/system/bt-agent.service >/dev/null <<'EOF'
[Unit]
Description=Bluetooth pairing agent (NoInputNoOutput)
After=bluetooth.service
Requires=bluetooth.service

[Service]
Type=simple
ExecStart=/usr/local/sbin/bt-agent.sh
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF
```

**BlueALSA** (A2DP sink endpoints + player routing into PipeWire):

```bash
sudo mkdir -p /etc/systemd/system/bluealsa.service.d
sudo tee /etc/systemd/system/bluealsa.service.d/a2dp.conf >/dev/null <<'EOF'
[Service]
ExecStart=
ExecStart=/usr/bin/bluealsa -p a2dp-sink
EOF
sudo tee /etc/systemd/system/bluealsa-aplay.service >/dev/null <<EOF
[Unit]
Description=BlueALSA A2DP player (BT audio to PipeWire)
After=bluealsa.service bluetooth.service
Requires=bluealsa.service

[Service]
Environment=XDG_RUNTIME_DIR=/run/user/$(id -u)
ExecStart=/usr/bin/bluealsa-aplay
Restart=always
RestartSec=3

[Install]
WantedBy=multi-user.target
EOF
```

**Enable everything** (plus linger, so PipeWire runs headless at boot):

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now rpiplay bt-agent bluealsa bluealsa-aplay
sudo loginctl enable-linger $USER
sudo reboot
```

## Step 7 — Pair the phone

1. Phone → Settings → Bluetooth → **"raspberrypi"** → Pair (auto-accepts)
2. On the Pi, mark it trusted so it reconnects at boot:

```bash
bluetoothctl devices Paired       # note the MAC
bluetoothctl trust <MAC>
```

3. Play music on the phone and select **raspberrypi** as the output.
   iPhone: Screen Mirroring → **RPiPlay** (AirPlay).

## Step 8 — Test

```bash
systemctl --user restart wireplumber   # if the sink list is empty after boot
pw-play /usr/share/sounds/alsa/Front_Center.wav   # or any test tone
systemctl is-active rpiplay bt-agent bluealsa bluealsa-aplay
```

---

# Troubleshooting

| Symptom | Cause / fix |
|---|---|
| Card missing (`aplay -l` shows only vc4hdmi) | overlay not applied, or the codec probe failed — check `dmesg \| grep pcm512x`; the patched module retries the probe for ~60 s |
| `ASoC error (-22)` at open | the runtime-PM bug — the patched module handles it; make sure the *patched* module is installed (out-of-tree taint message in dmesg) |
| Phone pairs but "connect for media audio" does nothing | no A2DP endpoints registered — `bluealsa`/`bluealsa-aplay` not running, or the bluez5 SPA plugin missing (`ls /usr/lib/*/spa-0.2/bluez5/`) |
| AirPlay tone has open/close clicks but no sound | DAC clock source unconfigured — the `patch_pcm512x_dacref.py` fix is missing |
| Pi drops off the LAN intermittently | WiFi power-save (`iw dev wlan0 set power_save off`), or a marginal power path (see below) |
| DAC power LED (C2) dims/dies, sinks vanish | the HAT's 3.3V rail brownouts from resistive header joints — reflow pins 1, 2, 17 and the GND return (6); continuity tests pass on bad joints, only a loaded voltage measurement reveals them |
| BT won't power on | the adapter may be rfkill-soft-blocked; `bt-agent.sh` clears it via `/sys/class/rfkill/rfkill*/soft` |

# Technical background

**The runtime-PM -22 bug.** On Raspberry Pi OS kernels ≥ 6.12, the PCM512x
codec's runtime resume (`pcm512x_resume` in `sound/soc/codecs/pcm512x.c`)
fails in `regulator_bulk_enable()`/`regcache_sync()` and the runtime-PM core
stores the error — every subsequent open re-fails with -22. Additionally, a
resume whose `regcache_sync` fails leaves the chip at reset defaults while
the driver's cache believes the old values applied: the DAC ended up with
volume at the reset default (−79.5 dB) and `DAC_REF` unconfigured — hence the
clicks-without-audio symptom and the third patch.

**Why the allo-boss overlay.** The `hifiberry-dacplus*` overlays reference
supply regulators whose runtime-PM handling triggers the failure, and their
non-Pro path runs the PCM5122 as I2S slave while the InnoMaker designs the
chip as clock master (MCLK/BCK ratio mismatch → clicks, no conversion). The
Allo Boss DAC overlay binds the same chip at I2C 0x4D with dummy regulators
and codec-master clocking. Resulting card name: `BossDAC`.

**Why BlueALSA instead of PipeWire's Bluetooth.** PipeWire's bluez5 monitor
on this image never loads its SPA plugin (verified: the plugin dlopens fine,
but WirePlumber's monitor never creates it), so no A2DP endpoints register
with bluetoothd. BlueALSA registers them independently and is the proven
lightweight path on the Zero. If a future image fixes the SPA loading, the
`51-bluez-a2dp.conf` settings (roles, SBC-XQ) become relevant again.

## Files in this directory

| File | Purpose |
|---|---|
| `provision.sh` | Automated version of the manual steps above |
| `boot-config-reference.txt` | Working `config.txt` snapshot (reference) |
| `etc-default-rpiplay` | `/etc/default/rpiplay` — service arguments |
| `systemd/rpiplay.service.pi` | rpiplay unit (`User=pi`, `EnvironmentFile`) |
| `systemd/rpiplay-pipewire.conf` | rpiplay drop-in: `XDG_RUNTIME_DIR` |
| `systemd/bt-agent.service` + `bt-agent.sh` | Persistent pairing agent |
| `systemd/bluealsa-a2dp.conf` | bluealsa daemon override (A2DP sink) |
| `systemd/bluealsa-aplay.service` | BT audio → PipeWire player unit |
| `modules/snd-soc-pcm512x-<kernel>.ko` | Prebuilt patched module (matching kernel only) |
| `pcm512x-fix/` | Patched source + Makefile + the three patch scripts |

## Trade-offs

* **AirPlay volume changes are ignored**: rpiplay renders through the ALSA
  `pipewire` PCM which has no mixer; volume is whatever the sink is set to.
  Bluetooth volume works (AVRCP + hardware mixer).
* The `-vr dummy` video renderer is used headless; for video output switch
  to `-vr gstreamer` (with `RPIPLAY_VIDEOSINK=kmssink` on a framebuffer
  console).
