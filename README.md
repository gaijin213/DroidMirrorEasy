# DroidMirror Easy

Wireless Android screen mirroring for Linux with **zero IPs, ports, or codes to type** — plus desktop notifications for calls, messages, and every app. The Android 11+ equivalent of iOS tap-to-mirror.

Built on [scrcpy](https://github.com/Genymobile/scrcpy) + adb, with a small Qt6 Widgets GUI (C++). No Electron, no Python, no npm.

---

## Features

### 1. One-scan QR pairing — no typing
`src/qrpairer.cpp:40` — generates a random `WIFI:T:ADB;S:droidmirror-XXXXXX;P:YYYYYYYYYYYY;;` payload (`src/qrpairer.cpp:44`), renders it with `qrencode` (`src/mainwindow.cpp:130`), polls `adb mdns services` (`src/adbmanager.cpp:184`) for the phone's `_adb-tls-pairing` advertisement, runs `adb pair <ip:port>` (`src/adbmanager.cpp:129`), then auto-connects via `_adb-tls-connect` (`src/qrpairer.cpp:116`).

### 2. 6-digit code fallback + auto-discovery
"Pair with code" tab — polls `adb mdns services` live, lists `_adb-tls-pairing` / `_adb-tls-connect` instances. No camera? Paste `IP:port` + 6-digit code manually. Manual `IP:port` connect under Advanced still works.

### 3. Desktop notifications from the phone
`src/notificationwatcher.cpp:34` — polls `adb -s <serial> shell dumpsys notification --noredact` every 3 s, parses `NotificationRecord pkg=... key=...` + `android.title` / `android.text` (`src/notificationwatcher.cpp:67`), primes on first poll then emits only **new/changed** notifications. Forwarded via Qt D-Bus `org.freedesktop.Notifications` to your desktop (`src/mainwindow.cpp:520`). No phone-side app.

### 4. Mirror controls
`src/mirrorlauncher.cpp:13` — builds `scrcpy --serial <serial>` args from `MirrorSettings` (`src/mirrorlauncher.h:7`): max-size, max-fps (`--max-fps`), bitrate (`--video-bit-rate`), `--no-audio`, `--turn-screen-off`, `--stay-awake`, `--fullscreen`, `--lock-video-orientation`. Live `adb devices -l` polling every 5 s (`src/mainwindow.cpp:30`), QSettings persistence, clean `QProcess` stop.

---

## How it was made

Qt6 Widgets (C++17) GUI around two CLI tools you already trust — **no wrappers, no bundled JS**:

- **adb (Google platform-tools)** — `src/adbmanager.cpp:1` resolves `~/android-sdk/platform-tools/adb` first (`src/adbmanager.cpp:20`), falls back to `PATH`/`adb.exe`. Wraps `adb devices -l` (`src/adbmanager.cpp:111`), `adb pair` (`src/adbmanager.cpp:127`), `adb connect` (`src/adbmanager.cpp:160`), `adb mdns services/check` (`src/adbmanager.cpp:184`). Auto-prefers Google adb because distro `android-tools` (Arch etc.) is often built **without mDNS**.
- **scrcpy** — `src/mirrorlauncher.cpp:37` launches `scrcpy --serial <id>` as a child `QProcess`, forwards settings, emits `mirrorStopped` on exit.
- **Qt6** — `droidmirror.pro:1` (`QT += core gui widgets dbus`), `CONFIG += c++17`. Main window (`src/mainwindow.cpp:1` / `src/mainwindow.h:1`) — two tabs (Easy / Advanced), QR label (`QLabel` + `qrencode` -> `QPixmap`), device table (`QTableWidget`), settings form.
- **qrencode** — CLI `qrencode -o <tmp.png>` called from `MainWindow::setQrImage` (`src/mainwindow.cpp:430`), loaded as pixmap. No extra Qt dependency.
- **D-Bus notifications** — `QDBusInterface org.freedesktop.Notifications.Notify` → native KDE/GNOME toasts.

---

## Requirements

### Runtime (needed to run the built binary)
| Tool | Why | Min version | Note |
|---|---|---|---|
| `adb` with mDNS (`adb mdns services`) | pair + connect over Wi-Fi | platform-tools 33+ | **Google platform-tools** `~/android-sdk/platform-tools/adb` preferred; distro `android-tools` often lacks mDNS |
| `scrcpy` | actual mirroring | 2.0+ | opens its own window `scrcpy --serial <id>` |
| `qrencode` | QR generation | 4.x | CLI `qrencode` |

### Build (needed to compile)
| Tool | Package (Arch) | Package (Debian/Ubuntu) | Package (Fedora) |
|---|---|---|---|
| Qt6 Widgets + D-Bus | `qt6-base` | `qt6-base-dev` + `qt6-base-dev-tools` | `qt6-qtbase-devel` |
| C++ toolchain | `base-devel` (g++/make/qmake6) | `build-essential` + `qmake6` | `gcc-c++` + `make` |
| `qmake6` | `qt6-base` provides it | `qt6-base-dev-tools` | `qt6-qtbase-devel` |
| `scrcpy`, `qrencode` | `scrcpy` `qrencode` | `scrcpy` `qrencode` | `scrcpy` `qrencode` |

Linux only. Phone needs **Android 11+**, same Wi-Fi as PC, Wireless debugging ON.

Check locally: `./install.sh --check`

---

## Project Structure

```
DroidMirrorEasy/
├─ droidmirror.pro               # qmake project — QT += core gui widgets dbus, c++17
├─ droidmirror-easy.desktop      # desktop entry (Exec=__BIN__/droidmirror-easy)
├─ install.sh                    # distro-aware installer (pacman/apt/dnf + platform-tools + build)
├─ .gitignore
├─ README.md
└─ src/
   ├─ main.cpp                   # QApplication + MainWindow
   ├─ mainwindow.h / .cpp        # Easy tab (QR), Advanced tab (pair/connect/table), settings, D-Bus toasts
   ├─ adbmanager.h / .cpp        # adb wrapper: devices, mdns, pair, connect, disconnect, resolveAdb()
   ├─ mirrorlauncher.h / .cpp    # scrcpy QProcess wrapper + MirrorSettings -> args
   ├─ qrpairer.h / .cpp          # WIFI:T:ADB payload + mDNS poll + pair/connect state machine
   └─ notificationwatcher.h/.cpp # dumpsys notification poller (3 s) -> PhoneNotification
```

---

## Install (one command)

```bash
bash <(curl -fsSL https://raw.githubusercontent.com/gaijin213/DroidMirrorEasy/main/install.sh)
```

or clone and run:

```bash
git clone https://github.com/gaijin213/DroidMirrorEasy.git
cd DroidMirrorEasy
./install.sh
```

Options:
```bash
./install.sh --check      # only verify deps, change nothing
./install.sh --yes        # assume yes to all prompts (CI)
./install.sh --no-install # never call the package manager
```

What it does:

- detects your package manager (`pacman` / `apt` / `dnf`) and offers to install the dependencies (`qt6-base`, `scrcpy`, `qrencode`, toolchain; `adb` is bundled via Google's platform-tools)
- downloads the official Google platform-tools `37.0.0` into `~/android-sdk/` — needed because distro adb packages are often built **without mDNS**, and the app auto-prefers this adb (`src/adbmanager.cpp:19`)
- builds the app (`qmake6 droidmirror.pro && make -j$(nproc)`) and installs it to `~/.local/bin` + your app menu (`~/.local/share/applications/droidmirror-easy.desktop`)
- checks `firewalld` for mDNS (UDP 5353) and warns if blocked
- appends `~/android-sdk/platform-tools` to `PATH` in `~/.profile` if missing

---

## First run (30 seconds, done once)

1. Phone → Settings → **Developer options** → **Wireless debugging** → ON
   (no Developer options? tap **Build number** 7 times)
2. In DroidMirror Easy press **Show QR code**
3. Phone → Wireless debugging → **Pair device with QR code** → scan it
4. Press **Mirror phone**

Later sessions: turn Wireless debugging back on, press Show QR code again (or use 6-digit code fallback).

---

## Building by hand

Prerequisites: Qt6 (Widgets + D-Bus), `scrcpy`, `qrencode`, adb with mDNS.

```bash
qmake6 droidmirror.pro && make -j$(nproc)
./droidmirror-easy
```

Override adb: `DROIDMIRROR_ADB=/path/to/adb ./droidmirror-easy` (`src/adbmanager.cpp:15`).

Install manually:
```bash
mkdir -p ~/.local/bin ~/.local/share/applications
install -m755 droidmirror-easy ~/.local/bin/
sed "s|__BIN__|$HOME/.local/bin|g" droidmirror-easy.desktop > ~/.local/share/applications/droidmirror-easy.desktop
```

---

## Troubleshooting

- **Phone never appears / QR times out** → mDNS needs UDP 5353 open. If firewalld is active: `sudo firewall-cmd --permanent --zone=public --add-service=mdns && sudo firewall-cmd --reload`. Phone and PC must be on the same Wi-Fi; corporate/guest Wi-Fi often blocks mDNS.
- **"Wireless discovery unavailable"** → the app is using a distro adb without mDNS. Re-run `install.sh` so it downloads the Google platform-tools, or download them yourself into `~/android-sdk/platform-tools/` (`install.sh:131`).
- **No desktop notifications** → the phone must be connected (Wireless debugging on) and the "Forward phone notifications" checkbox enabled. Only *new* notifications pop after the app starts; existing ones are ignored (`src/notificationwatcher.cpp:83` — priming).
- **Paired but no mirror window** → scrcpy opens its own window; check the taskbar, or run `scrcpy --serial <device>` manually to see the error. Ensure `scrcpy` is in `PATH` (`src/mirrorlauncher.cpp:48`).
- **adb not found** → `install.sh --check` or set `DROIDMIRROR_ADB`.

---

## Windows

Planned. The Windows build will bundle scrcpy, platform-tools, and qrencode exe, and swap the D-Bus notification call for native Windows toasts. Not done yet.

---

## License

MIT — do as you wish. No warranty.
