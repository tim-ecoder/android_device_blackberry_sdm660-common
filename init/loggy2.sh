#!/system/bin/sh

# Log file path (currently unused -- streaming to UART only)
LOGFILE="/cache/loggy2.log"

# UART device
UART="/dev/ttyMSM0"

# Truncate any previous boot's log file so it reflects only the current boot
rm -f "$LOGFILE"

# Ensure UART device exists and is writable
if [ ! -w "$UART" ]; then
    echo "UART device $UART not writable!"
    exit 1
fi

# Handle Ctrl+C and terminate cleanly
trap "echo 'Stopping log capture'; exit 0" INT TERM

# Read logcat line by line
logcat | while IFS= read -r line
do
    LOG_LINE="LC: $line"

    # Append to logfile
#    echo "$LOG_LINE" >> "$LOGFILE"

    # Send to UART
    echo "$LOG_LINE" > "$UART"
done
