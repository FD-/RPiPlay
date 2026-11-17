#!/bin/sh
# Keeps the Bluetooth adapter powered, discoverable and pairable, and keeps
# a NoInputNoOutput pairing agent registered (bluez-tools bt-agent daemon),
# so phones can find and pair with the Pi headlessly.
#
# Some images ship the adapter rfkill-soft-blocked and without the rfkill
# tool; clear the block directly via sysfs. The initial sleep lets
# bluetoothd finish settling after boot.
sleep 3
for rf in /sys/class/rfkill/rfkill*; do
    [ "$(cat "$rf/type" 2>/dev/null)" = "bluetooth" ] && echo 0 > "$rf/soft" 2>/dev/null
done

printf 'power on\npairable on\ndiscoverable on\ndiscoverable-timeout 0\nquit\n' | /usr/bin/bluetoothctl

exec /usr/bin/bt-agent --capability=NoInputNoOutput
