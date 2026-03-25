#!/usr/bin/env bash
# Build Blunder for mobile platforms (Android and/or iOS).
#
# Usage:
#   ./scripts/build-mobile.sh              # build all available targets
#   ./scripts/build-mobile.sh android      # android only (arm64 + x86_64)
#   ./scripts/build-mobile.sh ios          # ios only (device + simulator)
#   ./scripts/build-mobile.sh android-arm64  # single target
#
# The script auto-downloads the Android NDK if not already present.
# iOS builds require macOS with Xcode installed.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# --- Configuration ---
NDK_VERSION="r27"
NDK_FULL="android-ndk-${NDK_VERSION}"
NDK_INSTALL_DIR="${PROJECT_ROOT}/prefix/ndk"
JOBS="$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"

# --- Colors ---
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

info()  { echo -e "${GREEN}[mobile]${NC} $*"; }
warn()  { echo -e "${YELLOW}[mobile]${NC} $*"; }
error() { echo -e "${RED}[mobile]${NC} $*" >&2; }

# --- NDK download ---
ensure_ndk() {
    # Check if user already has an NDK
    if [ -n "${ANDROID_NDK_HOME:-}" ] && [ -d "$ANDROID_NDK_HOME" ]; then
        info "Using existing NDK: $ANDROID_NDK_HOME"
        return 0
    fi
    if [ -n "${ANDROID_NDK_ROOT:-}" ] && [ -d "$ANDROID_NDK_ROOT" ]; then
        export ANDROID_NDK_HOME="$ANDROID_NDK_ROOT"
        info "Using existing NDK: $ANDROID_NDK_HOME"
        return 0
    fi

    # Check if we already downloaded it
    local ndk_path="${NDK_INSTALL_DIR}/${NDK_FULL}"
    if [ -d "$ndk_path" ]; then
        export ANDROID_NDK_HOME="$ndk_path"
        info "Using cached NDK: $ANDROID_NDK_HOME"
        return 0
    fi

    # Download
    info "Downloading Android NDK ${NDK_VERSION}..."
    mkdir -p "$NDK_INSTALL_DIR"

    local os_tag
    case "$(uname -s)" in
        Linux)  os_tag="linux" ;;
        Darwin) os_tag="darwin" ;;
        *)      error "Unsupported OS for NDK download: $(uname -s)"; return 1 ;;
    esac

    local url="https://dl.google.com/android/repository/${NDK_FULL}-${os_tag}.zip"
    local zip_path="${NDK_INSTALL_DIR}/ndk.zip"

    if command -v curl &>/dev/null; then
        curl -fSL --progress-bar -o "$zip_path" "$url"
    elif command -v wget &>/dev/null; then
        wget -q --show-progress -O "$zip_path" "$url"
    else
        error "Neither curl nor wget found. Install one or set ANDROID_NDK_HOME manually."
        return 1
    fi

    info "Extracting NDK..."
    unzip -q -o "$zip_path" -d "$NDK_INSTALL_DIR"
    rm -f "$zip_path"

    export ANDROID_NDK_HOME="$ndk_path"
    info "NDK installed to: $ANDROID_NDK_HOME"
}

# --- Build a single target ---
build_target() {
    local name="$1"
    local preset="$2"

    info "Building ${name}..."
    cd "$PROJECT_ROOT"

    cmake --preset="$preset"
    cmake --build "build/${preset}" -j "$JOBS"

    # Install into build/<preset>/install for a self-contained output
    cmake --install "build/${preset}" --prefix "build/${preset}/install"

    # Verify output
    local lib
    if [[ "$name" == Android* ]]; then
        # Android: expect executable
        local exe="build/${preset}/blunder"
        if [ ! -f "$exe" ]; then
            error "Build failed: blunder executable not found for ${name}"
            return 1
        fi
        # Strip debug symbols for smaller binary
        local strip_tool="${ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/$(uname -s | tr '[:upper:]' '[:lower:]')-x86_64/bin/llvm-strip"
        if [ -x "$strip_tool" ]; then
            "$strip_tool" "$exe"
            info "Stripped: $exe"
        fi
        local size
        size="$(du -h "$exe" | cut -f1)"
        info "${name}: ${exe} (${size})"
    else
        lib="$(find "build/${preset}" -name 'libblunder.a' | head -1)"
        if [ -z "$lib" ]; then
            error "Build failed: libblunder.a not found for ${name}"
            return 1
        fi
        local size
        size="$(du -h "$lib" | cut -f1)"
        info "${name}: ${lib} (${size})"
    fi
}

# --- Package a target ---
package_target() {
    local name="$1"
    local preset="$2"
    local out_dir="${PROJECT_ROOT}/build/mobile-dist"

    mkdir -p "${out_dir}"

    local staging
    staging="$(mktemp -d)"

    if [[ "$name" == android-* ]]; then
        # Bundle executable + opening books + weights
        mkdir -p "${staging}/engine" "${staging}/books"
        cp "build/${preset}/blunder" "${staging}/engine/"
        cp books/*.bin "${staging}/books/" 2>/dev/null || true
        cp books/*.epd "${staging}/books/" 2>/dev/null || true
        if ls weights/*.bin 1>/dev/null 2>&1; then
            mkdir -p "${staging}/weights"
            cp weights/*.bin "${staging}/weights/"
        fi
    else
        # iOS: bundle library + headers
        mkdir -p "${staging}/lib" "${staging}/include"
        cp "$(find "build/${preset}" -name 'libblunder.a' | head -1)" "${staging}/lib/"
        cp source/*.h "${staging}/include/"
    fi

    local zip_path="${out_dir}/blunder-${name}.zip"
    (cd "$staging" && zip -qr "$zip_path" .)
    rm -rf "$staging"

    info "Packaged: ${zip_path}"
}

# --- Main ---
main() {
    local targets=("${@:-all}")

    # Resolve what to build
    local do_android_arm64=false
    local do_android_x86_64=false
    local do_ios=false
    local do_ios_sim=false

    for t in "${targets[@]}"; do
        case "$t" in
            all)
                do_android_arm64=true
                do_android_x86_64=true
                if [ "$(uname -s)" = "Darwin" ]; then
                    do_ios=true
                    do_ios_sim=true
                else
                    warn "Skipping iOS targets (requires macOS)"
                fi
                ;;
            android)
                do_android_arm64=true
                do_android_x86_64=true
                ;;
            android-arm64)  do_android_arm64=true ;;
            android-x86_64) do_android_x86_64=true ;;
            ios)
                do_ios=true
                do_ios_sim=true
                ;;
            ios-device)     do_ios=true ;;
            ios-simulator)  do_ios_sim=true ;;
            *)
                error "Unknown target: $t"
                echo "Valid targets: all, android, android-arm64, android-x86_64, ios, ios-device, ios-simulator"
                exit 1
                ;;
        esac
    done

    # Android builds
    if $do_android_arm64 || $do_android_x86_64; then
        ensure_ndk
    fi

    local failed=0

    if $do_android_arm64; then
        build_target "Android arm64" "android-arm64" && \
        package_target "android-arm64" "android-arm64" || ((failed++))
    fi
    if $do_android_x86_64; then
        build_target "Android x86_64" "android-x86_64" && \
        package_target "android-x86_64" "android-x86_64" || ((failed++))
    fi
    if $do_ios; then
        if [ "$(uname -s)" != "Darwin" ]; then
            error "iOS device builds require macOS"
            ((failed++))
        else
            build_target "iOS arm64" "ios" && \
            package_target "ios-arm64" "ios" || ((failed++))
        fi
    fi
    if $do_ios_sim; then
        if [ "$(uname -s)" != "Darwin" ]; then
            error "iOS simulator builds require macOS"
            ((failed++))
        else
            build_target "iOS Simulator" "ios-simulator" && \
            package_target "ios-simulator" "ios-simulator" || ((failed++))
        fi
    fi

    echo ""
    if [ "$failed" -eq 0 ]; then
        info "All builds succeeded. Artifacts in build/mobile-dist/"
        ls -lh "${PROJECT_ROOT}/build/mobile-dist/"*.zip 2>/dev/null || true
    else
        error "${failed} build(s) failed"
        exit 1
    fi
}

main "$@"
