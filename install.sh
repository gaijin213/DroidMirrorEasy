#!/usr/bin/env bash
# DroidMirror Easy - Linux installer (build from source)
# Downloads Google platform-tools (adb with mDNS), builds the Qt6 app,
# installs it to ~/.local/bin and adds it to the app menu.
#
#   ./install.sh [--yes] [--no-install] [--check]

set -euo pipefail

APP_NAME="droidmirror-easy"
VERSION="0.1.0"
PT_VERSION="37.0.0"
PLATFORM_TOOLS_URL="https://dl.google.com/android/repository/platform-tools_r${PT_VERSION}-linux.zip"

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN_DIR="${HOME}/.local/bin"
APP_DIR="${HOME}/.local/share/applications"
SDK_DIR="${HOME}/android-sdk"
PT_DIR="${SDK_DIR}/platform-tools"
PROFILE_MARKER="# droidmirror-easy platform-tools PATH"

ASSUME_YES=0
SKIP_INSTALL=0
CHECK_ONLY=0

for arg in "$@"; do
    case "$arg" in
        -y|--yes) ASSUME_YES=1 ;;
        --no-install) SKIP_INSTALL=1 ;;
        --check) CHECK_ONLY=1 ;;
        -h|--help)
            echo "Usage: $0 [--yes] [--no-install] [--check]"
            echo "  --yes          assume yes to all prompts"
            echo "  --no-install   never run the package manager"
            echo "  --check        only verify dependencies, change nothing"
            exit 0 ;;
        *) echo "Unknown option: $arg" >&2; exit 1 ;;
    esac
done

c_ok=$'\e[32m'; c_warn=$'\e[33m'; c_err=$'\e[31m'; c_dim=$'\e[2m'; c_rst=$'\e[0m'
info() { printf '%s\n' "${c_ok}==>${c_rst} $*"; }
note() { printf '%s\n' "${c_dim}$*${c_rst}"; }
warn() { printf '%s\n' "${c_warn}warning:${c_rst} $*"; }
die()  { printf '%s\n' "${c_err}error:${c_rst} $*" >&2; exit 1; }

have() { command -v "$1" >/dev/null 2>&1; }

ask() { # ask <prompt> -> yes/no
    [ "$ASSUME_YES" = 1 ] && return 0
    local a
    printf '%s [Y/n] ' "$1"
    read -r a
    case "${a:-y}" in y|Y|yes|YES|"") return 0 ;; *) return 1 ;; esac
}

echo
info "DroidMirror Easy installer v${VERSION}"
note "App: wireless Android mirroring + notification forwarding (scrcpy + adb)."
echo

# ---------------------------------------------------------------- package mgr
PM=""
INSTALL_PKGS=""
if have pacman; then
    PM="pacman"
    INSTALL_PKGS="qt6-base scrcpy android-tools qrencode base-devel"
elif have apt-get; then
    PM="apt"
    INSTALL_PKGS="qt6-base-dev qt6-base-dev-tools build-essential scrcpy android-sdk-platform-tools-common qrencode"
elif have dnf; then
    PM="dnf"
    INSTALL_PKGS="qt6-qtbase-devel gcc-c++ make scrcpy android-tools qrencode"
fi

# ---------------------------------------------------------------- check tools
QMAKE=""
for c in qmake6 qmake; do
    if have "$c"; then QMAKE="$c"; break; fi
done

missing=()
have "$QMAKE"   || missing+=("qmake6 (Qt6 build tools)")
have make       || missing+=("make")
have g++        || missing+=("g++")
have scrcpy     || missing+=("scrcpy")
have qrencode   || missing+=("qrencode")
# adb is provided by the platform-tools download below, but system adb is nice.

if [ "$CHECK_ONLY" = 1 ]; then
    if [ -n "$PM" ]; then
        info "Detected package manager: ${PM}"
        note "  packages: ${INSTALL_PKGS}"
    fi
    if [ "${#missing[@]}" -gt 0 ]; then
        for m in "${missing[@]}"; do printf '  %smissing%s %s\n' "${c_warn}" "${c_rst}" "$m"; done
        die "Missing dependencies above."
    fi
    [ -x "$PT_DIR/adb" ] && note "platform-tools already present at $PT_DIR"
    info "All dependencies satisfied."
    exit 0
fi

if [ "${#missing[@]}" -gt 0 ]; then
    warn "Missing: ${missing[*]}"
    if [ -n "$PM" ] && [ "$SKIP_INSTALL" = 0 ]; then
        if ask "Install them with ${PM}? (needs sudo)"; then
            case "$PM" in
                pacman) sudo pacman -S --needed $INSTALL_PKGS ;;
                apt)    sudo apt-get update && sudo apt-get install -y $INSTALL_PKGS ;;
                dnf)    sudo dnf install -y $INSTALL_PKGS ;;
            esac
        else
            note "Skipping. You must install them yourself: $INSTALL_PKGS"
        fi
    else
        note "Install these yourself, then re-run: $INSTALL_PKGS"
    fi
    # re-detect after (possibly) installing
    for c in qmake6 qmake; do
        if have "$c"; then QMAKE="$c"; break; fi
    done
    have "$QMAKE" || die "qmake6 not found after install. Install the Qt6 dev tools for your distro and re-run."
    have make || die "make not found."
    have g++ || die "g++ not found."
fi

[ -n "$QMAKE" ] || die "qmake6 not found."

# ---------------------------------------------------------------- platform-tools
if [ -x "$PT_DIR/adb" ]; then
    note "adb (Google platform-tools) already present at $PT_DIR"
else
    if [ "$SKIP_INSTALL" = 0 ] || ask "Download Google platform-tools (~9 MB) to ${SDK_DIR}?"; then
        info "Downloading platform-tools ${PT_VERSION}..."
        mkdir -p "$SDK_DIR" /tmp/droidmirror-pt
        TMPZ="$(mktemp /tmp/droidmirror-pt/pt-XXXXXX.zip)"
        if ! curl -fSL -o "$TMPZ" "$PLATFORM_TOOLS_URL"; then
            rm -f "$TMPZ"
            die "Download failed. Try again or place adb in PATH manually."
        fi
        unzip -q -o "$TMPZ" -d "$SDK_DIR"
        rm -f "$TMPZ"
        chmod +x "$PT_DIR/adb" 2>/dev/null || true
        note "Installed to $PT_DIR"
    else
        warn "adb will be missing. The app cannot work without it."
    fi
fi

# ---------------------------------------------------------------- PATH
if [ -x "$PT_DIR/adb" ] && ! grep -qsF "$PROFILE_MARKER" "$HOME/.profile" 2>/dev/null; then
    {
        echo "$PROFILE_MARKER"
        echo 'if [ -d "$HOME/android-sdk/platform-tools" ]; then'
        echo '  PATH="$HOME/android-sdk/platform-tools:$PATH"'
        echo 'fi'
    } >> "$HOME/.profile"
    note "Added platform-tools to PATH in ~/.profile (applies to new terminals)."
fi

# ---------------------------------------------------------------- build
info "Building (${QMAKE})..."
cd "$REPO_DIR"
if ! "$QMAKE" droidmirror.pro; then
    "$QMAKE" "$REPO_DIR/droidmirror.pro"
fi
make -j"$(nproc 2>/dev/null || echo 2)"

[ -x "$REPO_DIR/$APP_NAME" ] || die "Build failed - no $APP_NAME produced."

# ---------------------------------------------------------------- install
info "Installing to ${BIN_DIR}"
mkdir -p "$BIN_DIR" "$APP_DIR"
install -m755 "$REPO_DIR/$APP_NAME" "$BIN_DIR/$APP_NAME"

DESKTOP_SRC="$REPO_DIR/$APP_NAME.desktop"
if [ -f "$DESKTOP_SRC" ]; then
    sed "s|__BIN__|${BIN_DIR}|g" "$DESKTOP_SRC" > "$APP_DIR/$APP_NAME.desktop"
    chmod 644 "$APP_DIR/$APP_NAME.desktop"
    note "App menu entry: $APP_DIR/$APP_NAME.desktop"
else
    warn "No $APP_NAME.desktop found in repo; skipping app-menu entry."
fi

# ---------------------------------------------------------------- firewall
if have firewall-cmd && firewall-cmd --state >/dev/null 2>&1; then
    if ! firewall-cmd --list-services --zone=public 2>/dev/null | grep -qw mdns; then
        warn "firewalld is active but blocks mDNS. To allow phone discovery, run:"
        echo "  sudo firewall-cmd --permanent --zone=public --add-service=mdns && sudo firewall-cmd --reload"
    fi
fi

echo
info "Done! Run the app from the menu or:"
echo "    ${BIN_DIR}/${APP_NAME}"
echo
note "First run: phone > Settings > Developer options > Wireless debugging > ON,"
note "then in the app press 'Show QR code' and scan with 'Pair device with QR code'."
