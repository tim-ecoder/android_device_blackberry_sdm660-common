#!/system/bin/sh
/system/bin/dmesg > /cache/kmsg_boot.txt
exec /system/bin/logcat -b all -f /cache/logcat_boot.txt
