#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
firmware_dir="$(cd "${script_dir}/.." && pwd)"

# shellcheck disable=SC1091
source "${firmware_dir}/toolchain.lock"

if ! command -v idf.py >/dev/null 2>&1; then
    echo "ERROR: idf.py is not available. Install and export ESP-IDF ${ESP_IDF_VERSION}." >&2
    exit 1
fi

if ! command -v cmake >/dev/null 2>&1; then
    echo "ERROR: cmake is not available." >&2
    exit 1
fi

if ! command -v ninja >/dev/null 2>&1; then
    echo "ERROR: ninja is not available." >&2
    exit 1
fi

actual_version="$(idf.py --version)"
case "${actual_version}" in
    *"${ESP_IDF_VERSION#v}"*)
        echo "PASS: ${actual_version}"
        ;;
    *)
        echo "ERROR: expected ESP-IDF ${ESP_IDF_VERSION}, got ${actual_version}" >&2
        exit 1
        ;;
esac

echo "Pinned HaLow component: ${MORSE_HALOW_COMPONENT}=${MORSE_HALOW_VERSION}"
echo "Pinned board profile: ${MORSE_BOARD_PROFILE}"
echo "Host CMake: $(cmake --version | sed -n '1p') (macOS verified: ${HOST_CMAKE_VERIFIED_MACOS})"
echo "Host Ninja: $(ninja --version) (macOS verified: ${HOST_NINJA_VERIFIED_MACOS})"
