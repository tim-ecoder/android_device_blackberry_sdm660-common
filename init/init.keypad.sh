#!/vendor/bin/sh
# Read keyboard layout type from traceability partition and set keypad name.
# trace_util r 8 returns: "N layout_name" (e.g. "1 qwerty", "3 qwertz")
# Write the number directly to sysfs to switch input device name.

KL=$(/vendor/bin/trace_util r 8 2>/dev/null)
NUM=${KL%% *}
if [ -n "$NUM" ]; then
    echo "$NUM" > /sys/devices/keypad/kl
    setprop ro.hwf.keypadlanguage "$KL"
fi
