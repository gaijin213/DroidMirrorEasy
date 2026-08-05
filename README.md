# DroidMirror Easy

Wireless Android screen mirroring for Linux with **zero IPs, ports, or codes to
type** — plus desktop notifications for calls, messages, and every app. The
Android 11+ equivalent of iOS tap-to-mirror.

Built on [scrcpy](https://github.com/Genymobile/scrcpy) + adb, with a small Qt6
Widgets GUI (C++). No Electron, no Python, no npm.

## Install (one command)

```
bash <(curl -fsSL https://raw.githubusercontent.com/gaijin213/droidmirror-easy/main/install.sh)
```

or clone and run:

```
git clone https://github.com/gaijin213/droidmirror-easy.git
cd droidmirror-easy
./install.sh
```

The installer:

- detects your package manager (pacman / apt / dnf) and offers to install the
  dependencies (`qt6-base`, `scrcpy`, `qrencode`, a C++ toolchain; `adb` is
  bundled via Google's platform-tools)
- downloads the official Google platform-tools into `~/android-sdk/` — needed
  because distro adb packages (e.g. Arch's `android-tools`) are often built
  **without mDNS**, and the app auto-prefers this adb
- builds the app and installs it to `~/.local/bin` + your app menu

## First run (30 seconds, done once)

1. Phone → Settings → **Developer options** → **Wireless debugging** → ON
   (no Developer options? tap **Build number** 7 times)
2. In DroidMirror Easy press **Show QR code**
3. Phone → Wireless debugging → **Pair device with QR code** → scan it
4. Press **Mirror phone**

Later sessions: turn Wireless debugging back on, press Show QR code again.

## Features

- QR-code pairing (no typing) — the Android 11+ tap-to-mirror equivalent
- 6-digit-code fallback with automatic network discovery (no camera? no problem)
- **Desktop notifications from the phone** — calls, SMS, WhatsApp, any app —
  forwarded over the adb connection, no phone-side app needed
- Live device list + mirror controls (resolution, FPS, bitrate, orientation,
  audio, screen-off, stay-awake, fullscreen)
- Manual IP/port/code panel under "Advanced" for power users

## Building by hand

```
qmake6 droidmirror.pro && make
./droidmirror-easy
```

Requires: Qt6 (Widgets + D-Bus), scrcpy, qrencode, and an adb with mDNS
(system adb, or the Google platform-tools in `~/android-sdk/platform-tools`).

## Troubleshooting

- **Phone never appears / QR times out** → mDNS needs UDP 5353 open. If
  firewalld is active: `sudo firewall-cmd --permanent --zone=public
  --add-service=mdns && sudo firewall-cmd --reload`. Phone and PC must be on
  the same Wi-Fi; corporate/guest Wi-Fi often blocks mDNS.
- **"Wireless discovery unavailable"** → the app is using a distro adb without
  mDNS. Re-run `install.sh` so it downloads the Google platform-tools, or
  download them yourself into `~/android-sdk/platform-tools/`.
- **No desktop notifications** → the phone must be connected (Wireless
  debugging on) and the "Forward phone notifications" checkbox enabled. Only
  *new* notifications pop after the app starts; existing ones are ignored.
- **Paired but no mirror window** → scrcpy opens its own window; check the
  taskbar, or run `scrcpy --serial <device>` manually to see the error.

## Windows

Planned. The Windows build will bundle scrcpy, platform-tools, and qrencode
exe, and swap the D-Bus notification call for native Windows toasts. Not done
yet.
