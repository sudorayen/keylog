#!/usr/bin/env bash
# uninstall.sh — remove the keylogger completely
set -euo pipefail
PLIST=/Library/LaunchDaemons/com.ollie.keylog.plist
BIN=/usr/local/bin/keylog
LOG="$HOME/Downloads/keylog.log"

echo "[*] stopping daemon..."
sudo launchctl unload "$PLIST" 2>/dev/null || sudo launchctl stop com.ollie.keylog 2>/dev/null || true

echo "[*] removing files..."
sudo rm -f "$PLIST" "$BIN" "$LOG"

echo "[*] (optional) remove Input Monitoring entry:"
echo "    System Settings -> Privacy & Security -> Input Monitoring -> select keylog -> '-'"
echo
echo "[+] uninstalled. setup.sh / start.sh / stop.sh / uninstall.sh can be deleted manually."