#!/usr/bin/env bash
# full_dump.sh — print the entire keylog
LOG="$HOME/Downloads/keylog.log"
if [ ! -f "$LOG" ]; then
    echo "[!] no log yet: $LOG"
    echo "    run setup.sh and grant Input Monitoring first."
    exit 1
fi
echo "[*] full dump of $LOG"
echo "----------------------------------------"
cat "$LOG"
echo "----------------------------------------"
echo "[*] lines: $(wc -l < "$LOG")"