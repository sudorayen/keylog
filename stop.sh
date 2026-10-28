#!/usr/bin/env bash
# stop.sh — stop the keylogger daemon
set -euo pipefail
PLIST=/Library/LaunchDaemons/com.ollie.keylog.plist

echo "[*] unloading daemon..."
sudo launchctl unload "$PLIST" 2>/dev/null || sudo launchctl stop com.ollie.keylog 2>/dev/null || true
sleep 0.3

if sudo launchctl list | grep -q com.ollie.keylog; then
    echo "[!] still running. force: sudo killall keylog"
else
    echo "[+] stopped."
fi