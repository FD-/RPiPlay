#!/bin/bash
# One-command deployment of RPiPlay (+ the whole speaker/AirPlay stack) to a
# Raspberry Pi over SSH, from a workstation that has this repository.
#
# Usage:
#   ./deploy/one-command-deploy.sh [user@]pi-host [--skip-verify] [--no-reboot]
#
# Examples:
#   ./deploy/one-command-deploy.sh pi@10.0.0.16
#   ./deploy/one-command-deploy.sh raspberrypi-new
#
# What it does:
#   1. Checks SSH connectivity (guides you to ssh-copy-id if needed)
#   2. Inspects the Pi (arch, kernel, RAM)
#   3. rsyncs this repository to ~/RPiPlay on the Pi
#   4. Runs deploy/provision.sh interactively (type the sudo password when asked)
#   5. Offers a reboot (needed to activate the DAC overlay)
#   6. After the reboot: verifies card, services and advertisement
set -u

REPO="$(cd "$(dirname "$0")/.." && pwd)"
SKIP_VERIFY=0
NO_REBOOT=0
TARGET=""

for arg in "$@"; do
    case "$arg" in
        --skip-verify) SKIP_VERIFY=1 ;;
        --no-reboot)   NO_REBOOT=1 ;;
        -h|--help)     sed -n '2,14p' "$0"; exit 0 ;;
        *)             TARGET="$arg" ;;
    esac
done
if [ -z "$TARGET" ]; then
    echo "Usage: $0 [user@]pi-host [--skip-verify] [--no-reboot]"; exit 1
fi

echo "== [1/6] Checking SSH connectivity to $TARGET =="
if ! ssh -o BatchMode=yes -o ConnectTimeout=8 "$TARGET" 'echo ok' >/dev/null 2>&1; then
    echo "  SSH key auth not set up yet. Run:"
    echo "    ssh-copy-id -i ~/.ssh/id_ed25519.pub $TARGET"
    echo "  (enter the Pi password once), then re-run this script."
    exit 1
fi
echo "  connected"

echo "== [2/6] Inspecting the Pi =="
ssh "$TARGET" '
    echo "  host: $(hostname) | model: $(cat /proc/device-tree/model 2>/dev/null | tr -d "\0")"
    echo "  arch: $(uname -m) | kernel: $(uname -r) | cores: $(nproc)"
    echo "  os:   $(head -1 /etc/os-release)"
    free -m | awk "/Mem:/ {print \"  ram: \" \$2 \" MB total\"}"
    [ -d /lib/modules/$(uname -r)/build ] && echo "  kernel headers: present" || echo "  kernel headers: will be installed by provision.sh"
'
NPROC=$(ssh "$TARGET" nproc)
RAM=$(ssh "$TARGET" free -m | awk "/^Mem:/ {print \$2}")
if [ "$NPROC" -le 1 ]; then
    echo "  NOTE: single core - the build takes a while (30-60 min). Let it run."
fi

echo "== [3/6] Syncing the repository =="
rsync -a --delete --exclude build --exclude .git "$REPO/" "$TARGET:RPiPlay/" || {
    echo "rsync failed (network?)"; exit 1; }

echo "== [4/6] Running provision.sh on the Pi (interactive - enter the sudo password) =="
ssh -t "$TARGET" 'cd ~/RPiPlay/deploy && ./provision.sh' || {
    echo "provision.sh failed - see the output above"; exit 1; }

if [ "$NO_REBOOT" -eq 0 ]; then
    echo "== [5/6] Rebooting to activate the DAC overlay =="
    ssh -t "$TARGET" 'sudo /sbin/reboot' 2>/dev/null || {
        echo "  reboot needs the sudo password - run it yourself:"; echo "    ssh -t $TARGET sudo reboot"; NO_REBOOT=1; }
    if [ "$NO_REBOOT" -eq 0 ]; then
        echo "  waiting for the Pi to come back..."
        for i in $(seq 1 40); do
            sleep 5
            if ssh -o BatchMode=yes -o ConnectTimeout=4 "$TARGET" 'echo up' >/dev/null 2>&1; then
                echo "  Pi is back (after ~$((i*5))s)"; break
            fi
        done
    fi
fi

if [ "$SKIP_VERIFY" -eq 0 ]; then
    echo "== [6/6] Verifying =="
    ssh "$TARGET" '
        FAIL=0
        for s in rpiplay bt-agent bluealsa bluealsa-aplay; do
            if systemctl is-active $s >/dev/null 2>&1; then echo "  $s: active"; else echo "  $s: NOT ACTIVE"; FAIL=1; fi
        done
        if aplay -l 2>/dev/null | grep -qviE "hdmi"; then :; fi
        aplay -l 2>/dev/null | grep -iE "BossDAC|hifiberry|Boss" >/dev/null 2>&1 \
            && echo "  DAC card: present" || { echo "  DAC card: MISSING (check the HAT seating + the overlay)"; FAIL=1; }
        systemctl is-enabled rpiplay >/dev/null 2>&1 && echo "  boot persistence: enabled"
        [ $FAIL -eq 0 ] || exit 1
    ' && echo "  services OK"
fi

cat <<'EOF'

=== Deployment complete ===

  Pair the phone:  Phone Bluetooth settings -> "raspberrypi" -> pair
  Then trust it:   bluetoothctl devices Paired && bluetoothctl trust <MAC>
  Play:            phone output -> "raspberrypi"  (Bluetooth)
                    iPhone: Screen Mirroring -> "RPiPlay"  (AirPlay)

  Debug:           ssh <pi> ; journalctl -u rpiplay -u bluealsa-aplay -f
  Documentation:   RPiPlay/deploy/README.md on the Pi (or in this repo)
EOF
