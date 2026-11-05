#!/vendor/bin/sh
#
# The sensors HAL (sensors.ssc.so) waits for every datatype in its mandatory
# list before it registers ISensors, re-requesting the missing ones every
# 250 ms for up to 8 s. Lists written by older stock firmware contain
# mot_tap, which the SLPI does not publish, so every boot stalled for 8 s
# (and system_server with it, in DeviceStateManagerService). Drop the stale
# entry before the HAL starts; the rest of the list is left as is.
#

list=/mnt/vendor/persist/sensors/sensors_list.txt

grep -qx mot_tap "$list" 2>/dev/null || exit 0

if grep -vx mot_tap "$list" > "$list.tmp"; then
    chmod 0600 "$list.tmp" && mv -f "$list.tmp" "$list"
fi
rm -f "$list.tmp"
