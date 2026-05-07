#!/system/bin/sh
# One-shot boot capture. Truncates /cache/{kmsg,logcat}_boot.txt so they
# reflect ONLY this boot. logcat -d dumps whatever is in the ring buffer
# at script-run-time and exits -- does NOT stream new entries.
rm -f /cache/kmsg_boot.txt /cache/logcat_boot.txt
/system/bin/dmesg > /cache/kmsg_boot.txt
/system/bin/logcat -b all -d > /cache/logcat_boot.txt
