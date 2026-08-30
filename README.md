# DroidMirror Easy

**Mirror your Android phone on your Linux screen — wirelessly, no typing, no cables.**
Scan a QR code with your phone, press a button, and your screen is mirrored. You also get
desktop notifications for calls, texts and every app on your phone.

Works on **any Linux** (Arch, Ubuntu, Fedora, Mint, Pop!_OS, Debian...). No compiling.
Built on [scrcpy](https://github.com/Genymobile/scrcpy) + adb, with a small Qt6 app.

---

## Install (the easy way — no compiling)

Just copy-paste this into a terminal and press Enter:

```bash
bash <(curl -fsSL https://raw.githubusercontent.com/gaijin213/DroidMirrorEasy/main/install.sh)
```

The installer does everything for you:

1. **Installs the needed apps** (`scrcpy`, `qrencode`, `Qt6`) using your system's package
   manager — it auto-detects Arch / Ubuntu / Debian / Fedora.
   (It will ask for your password once, that's normal.)
2. **Downloads `adb`** (Google's platform-tools) into `~/android-sdk/` — adb is what
   talks to your phone.
3. **Downloads a ready-built DroidMirror Easy** binary and puts it in your app menu.

When it's done, find **DroidMirror Easy** in your app menu, or run:

```bash
~/.local/bin/droidmirror-easy
```

> The installer asks before installing anything. If you don't have `curl`, install it
> first (`sudo apt install curl`, `sudo pacman -S curl`, or `sudo dnf install curl`).

---

## First run (30 seconds, do this once)

1. On your **phone**: open **Settings → About phone**. Tap **Build number** **7 times**
   until it says "You are now a developer".
2. **Settings → Developer options → Wireless debugging** → turn it **ON**.
   (When it asks to allow wireless debugging on this network, say yes.)
3. On your **PC**: open DroidMirror Easy and press **Show QR code**.
4. On your **phone**: tap **Pair device with QR code** and scan the code on your screen.
5. Press **Mirror phone**. Your phone appears on your PC. Done!

**Next time:** just turn Wireless debugging back on and press **Show QR code** again.

Phone needs **Android 11 or newer** and must be on the **same Wi-Fi** as your PC.

---

## What you can do

- Mirror your phone screen with **zero IPs, ports or codes to type**
- See **calls, texts and app notifications on your PC** (no phone app needed)
- No camera? Use **"Pair with code"** — automatic discovery + 6-digit code
- Mirror controls: resolution, FPS, bitrate, orientation, audio, screen-off,
  stay-awake, fullscreen
- Manual IP/port connect under **Advanced** for power users

---

## Troubleshooting (usually a 2-second fix)

- **"Phone never appears" / QR times out**
  - Phone and PC must be on the **same Wi-Fi**. Public/corporate/guest Wi-Fi often
    blocks this — use your home Wi-Fi or a phone hotspot.
  - On Fedora, if `firewalld` is running, allow discovery:
    `sudo firewall-cmd --permanent --zone=public --add-service=mdns && sudo firewall-cmd --reload`
- **"Wireless discovery unavailable"**
  - Your distro's adb can't do wireless discovery. Re-run the installer so it downloads
    Google's platform-tools (they live in `~/android-sdk/platform-tools/`).
- **No desktop notifications**
  - Wireless debugging must be ON and the phone connected. Only **new** notifications
    pop up after the app starts — old ones are ignored.
- **Paired but no mirror window**
  - scrcpy opens its own window. Check the taskbar. Or open a terminal and run
    `scrcpy --serial <device>` to see the exact error.

---

## Requirements

You normally never need to read this — the installer handles it. But here's the list:

| Needed to run | What for |
|---|---|
| Qt6 (libs) | the app's window/buttons |
| `scrcpy` | the actual screen mirroring |
| `qrencode` | drawing the QR code |
| `adb` (Google platform-tools) | talking to your phone over Wi-Fi |

That's it. No Python, no Node, no Electron.

---

## Build from source (advanced)

Only needed if you're a developer or your distro is unusual.

```bash
git clone https://github.com/gaijin213/DroidMirrorEasy.git
cd DroidMirrorEasy
./install.sh --source
```

Install from the release binary only:

```bash
mkdir -p ~/.local/bin
curl -fSL -o ~/.local/bin/droidmirror-easy \
  https://github.com/gaijin213/DroidMirrorEasy/releases/latest/download/droidmirror-easy-linux-x86_64
chmod +x ~/.local/bin/droidmirror-easy
```

Verify the installer is healthy without changing anything:

```bash
./install.sh --check
```

---

## Project layout

```
DroidMirrorEasy/
├─ droidmirror.pro               # qmake project — Qt6 Widgets + D-Bus, C++17
├─ droidmirror-easy.desktop      # app-menu entry
├─ install.sh                    # noob installer: prebuilt binary + deps, source fallback
└─ src/
   ├─ main.cpp                   # app entry point
   ├─ mainwindow.h/.cpp          # UI: QR tab, Advanced tab, settings, notifications
   ├─ adbmanager.h/.cpp          # talks to adb (devices, mDNS, pair, connect)
   ├─ mirrorlauncher.h/.cpp      # starts scrcpy
   ├─ qrpairer.h/.cpp            # makes the QR code + auto-pairs
   └─ notificationwatcher.h/.cpp # forwards phone notifications to your desktop
```

---

## How it works (5-minute version)

1. **QR pairing** (`src/qrpairer.cpp:44`) — the app creates a secret password you never
   see and draws it as a `WIFI:T:ADB` QR code. Your phone scans it, then advertises
   itself ("I'm here, port 37000"). The app finds it via mDNS (`adb mdns services`) and
   pairs automatically. No typing at any point.
2. **Mirroring** (`src/mirrorlauncher.cpp:13`) — the app starts `scrcpy --serial <phone>`
   with your chosen settings (resolution, FPS, audio...).
3. **Notifications** (`src/notificationwatcher.cpp:34`) — every 3 seconds it asks the
   phone what notifications it has (`dumpsys notification`) and shows new ones on your
   desktop via the Linux notification system.

That's the whole trick — scrcpy + adb already do the heavy lifting; this app makes it a
one-scan experience and adds notifications.

---

## Windows

Planned. The Windows version will bundle everything and use native toasts. Not done yet.

---

## License

MIT — do as you wish. No warranty.