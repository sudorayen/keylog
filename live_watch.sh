#!/usr/bin/env bash
# live_watch.sh — follow the keylog in real time
LOG="$HOME/Downloads/keylog.log"
if [ ! -f "$LOG" ]; then
    echo "[!] no log yet: $LOG"
    echo "    run setup.sh and grant Input Monitoring first."
    exit 1
fi
echo "[*] watching $LOG  (Ctrl+C to stop)"
tail -f "$LOG"