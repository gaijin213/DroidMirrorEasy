#!/usr/bin/env bash
# DroidMirror Easy - Linux installer for noobs
#   Preferred path: download the PREBUILT binary from the GitHub release,
#   fetch Google platform-tools (adb with mDNS), install runtime deps.
#   Fallback: build from source (--source flag) if your distro is unsupported.
#
#   ./install.sh [--yes] [--no-install] [--check] [--source]

set -euo pipefail

APP_NAME="droidmirror-easy"
VERSION="0.1.0"
RELEASE_VERSION="0.2.0"
BIN_URL="https://github.com/gaijin213/DroidMirrorEasy/releases/download/v${RELEASE_VERSION}/droidmirror-easy-linux-x86_64"
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
FORCE_SOURCE=0

for arg in "$@"; do
    case "$arg" in
        -y|--yes) ASSUME_YES=1 ;;
        --no-install) SKIP_INSTALL=1 ;;
        --check) CHECK_ONLY=1 ;;
        --source) FORCE_SOURCE=1 ;;
        -h|--help)
            echo "Usage: $0 [--yes] [--no-install] [--check] [--source]"
            echo "  --yes          assume yes to all prompts"
            echo "  --no-install   never run the package manager"
            echo "  --check        only verify dependencies, change nothing"
            echo "  --source       build from source instead of using the prebuilt binary"
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
info "DroidMirror Easy installer v${VERSION} (release ${RELEASE_VERSION})"
note "App: wireless Android mirroring + notification forwarding (scrcpy + adb)."
echo

# ---------------------------------------------------------------- package mgr
# Runtime deps are needed to RUN the app (Qt6 libs, scrcpy, qrencode).
# Build deps are only needed for the --source path.
PM=""
RUNTIME_PKGS=""
BUILD_PKGS=""
if have pacman; then
    PM="pacman"
    RUNTIME_PKGS="qt6-base scrcpy qrencode unzip"
    BUILD_PKGS="base-devel"
elif have apt-get; then
    PM="apt"
    RUNTIME_PKGS="qt6-base-dev qt6-base-dev-tools scrcpy qrencode unzip"
    BUILD_PKGS="build-essential"
elif have dnf; then
    PM="dnf"
    RUNTIME_PKGS="qt6-qtbase-devel scrcpy qrencode unzip"
    BUILD_PKGS="gcc-c++ make"
fi

# ---------------------------------------------------------------- check tools
missing_runtime=()
have scrcpy   || missing_runtime+=("scrcpy")
have qrencode || missing_runtime+=("qrencode")

# unzip_extract <archive> <destdir>  (unzip, falling back to python3)
unzip_extract() {
    local tmp
    tmp="$(mktemp -d /tmp/droidmirror-ue-XXXXXX)"
    if have unzip; then
        unzip -q -o "$1" -d "$2"
    elif have python3; then
        python3 -m zipfile -e "$1" "$2"
    else
        # manual fallback: keep the zip and note it
        warn "No unzip or python3 to extract the zip. It is saved at: $1"
        return 1
    fi
    rm -rf "$tmp"
}

missing_build=()
QMAKE=""
for c in qmake6 qmake; do
    if have "$c"; then QMAKE="$c"; break; fi
done
have "$QMAKE" || missing_build+=("qmake6 (Qt6 build tools)")
have make     || missing_build+=("make")
have g++      || missing_build+=("g++")

if [ "$CHECK_ONLY" = 1 ]; then
    if [ -n "$PM" ]; then
        info "Detected package manager: ${PM}"
        note "  runtime packages: ${RUNTIME_PKGS}"
        note "  build packages:   ${BUILD_PKGS}"
    fi
    if [ "${#missing_runtime[@]}" -gt 0 ]; then
        for m in "${missing_runtime[@]}"; do printf '  %smissing%s %s\n' "${c_warn}" "${c_rst}" "$m"; done
        die "Missing runtime dependencies above."
    fi
    [ -x "$PT_DIR/adb" ] && note "platform-tools already present at $PT_DIR"
    info "All dependencies satisfied."
    exit 0
fi

# ------------------------------------------------------ install runtime deps
if [ "${#missing_runtime[@]}" -gt 0 ]; then
    warn "Missing: ${missing_runtime[*]}"
    if [ -n "$PM" ] && [ "$SKIP_INSTALL" = 0 ]; then
        if ask "Install them with ${PM}? (needs sudo)"; then
            case "$PM" in
                pacman) sudo pacman -S --needed $RUNTIME_PKGS ;;
                apt)    sudo apt-get update && sudo apt-get install -y $RUNTIME_PKGS ;;
                dnf)    sudo dnf install -y $RUNTIME_PKGS ;;
            esac
        else
            note "Skipping. You must install them yourself: $RUNTIME_PKGS"
        fi
    else
        note "Install these yourself, then re-run: $RUNTIME_PKGS"
    fi
    have scrcpy   || die "scrcpy not found. Install it and re-run."
    have qrencode || die "qrencode not found. Install it and re-run."
fi

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
        if ! unzip_extract "$TMPZ" "$SDK_DIR"; then
            rm -f "$TMPZ"
            die "Could not extract platform-tools. Install 'unzip' and re-run."
        fi
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

# ---------------------------------------------------------------- get binary
BIN_FILE="${BIN_DIR}/${APP_NAME}"
obtain_binary() {
    if [ "$FORCE_SOURCE" = 1 ]; then
        build_source
        return 0
    fi
    info "Downloading prebuilt binary (release ${RELEASE_VERSION})..."
    TMPB="$(mktemp /tmp/droidmirror-bin-XXXXXX)"
    if curl -fSL -o "$TMPB" "$BIN_URL"; then
        chmod 755 "$TMPB"
        "$TMPB" --help >/dev/null 2>&1 || true
        if file "$TMPB" | grep -q ELF; then
            note "Prebuilt binary looks good."
            mv -f "$TMPB" "$BIN_FILE"
            return 0
        fi
        rm -f "$TMPB"
    fi
    warn "Prebuilt download failed. Falling back to building from source..."
    build_source
}

build_source() {
    if [ "${#missing_build[@]}" -gt 0 ]; then
        warn "Missing build tools: ${missing_build[*]}"
        if [ -n "$PM" ] && [ "$SKIP_INSTALL" = 0 ]; then
            if ask "Install them with ${PM}? (needs sudo)"; then
                case "$PM" in
                    pacman) sudo pacman -S --needed $BUILD_PKGS ;;
                    apt)    sudo apt-get update && sudo apt-get install -y $BUILD_PKGS ;;
                    dnf)    sudo dnf install -y $BUILD_PKGS ;;
                esac
            fi
        fi
        for c in qmake6 qmake; do
            if have "$c"; then QMAKE="$c"; break; fi
        done
        have "$QMAKE" || die "qmake6 not found. Install the Qt6 dev tools for your distro and re-run."
        have make || die "make not found."
        have g++  || die "g++ not found."
    fi

    info "Building from source (${QMAKE})..."
    if [ ! -f "$REPO_DIR/src/main.cpp" ]; then
        die "Source files not found. Clone the full repo or re-run the curl command."
    fi
    cd "$REPO_DIR"
    if ! "$QMAKE" droidmirror.pro; then
        "$QMAKE" "$REPO_DIR/droidmirror.pro"
    fi
    make -j"$(nproc 2>/dev/null || echo 2)"
    [ -x "$REPO_DIR/$APP_NAME" ] || die "Build failed - no $APP_NAME produced."
    install -m755 "$REPO_DIR/$APP_NAME" "$BIN_FILE"
}

# ---------------------------------------------------------------- install
mkdir -p "$BIN_DIR" "$APP_DIR"
obtain_binary
[ -x "$BIN_FILE" ] || die "Install failed - no $APP_NAME produced."

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