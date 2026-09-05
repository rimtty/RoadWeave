#!/usr/bin/env bash
# Generate the official Morse Micro HaLow examples used in P0-A Gate 4 into firmware/halow_examples/
# (softap, sta_connect, iperf). Generated projects are not committed (.gitignore).
set -euo pipefail
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
firmware_dir="$(cd "${script_dir}/.." && pwd)"
out="${firmware_dir}/halow_examples"
# shellcheck disable=SC1091
source "${firmware_dir}/toolchain.lock"
"${script_dir}/check-toolchain.sh"
mkdir -p "${out}"
for ex in ${1:-softap sta_connect iperf}; do
    if [[ -e "${out}/${ex}" ]]; then echo "skip ${ex}: exists"; continue; fi
    ( cd "${out}" && idf.py create-project-from-example "${MORSE_HALOW_COMPONENT}=${MORSE_HALOW_VERSION}:${ex}" )
    # The examples' S1G channel / op-class have no Kconfig default and break the build until set.
    # Put build-only placeholders in; the real values go in with the country code at Gate 4 (menuconfig).
    if grep -q "config S1G_CHANNEL" "${out}/${ex}/main/Kconfig.projbuild" 2>/dev/null; then
        printf '\n# RoadWeave build placeholders: set real S1G channel/op-class + country code via menuconfig before flashing\nCONFIG_S1G_CHANNEL=0\nCONFIG_S1G_OPCLASS=0\n' >> "${out}/${ex}/sdkconfig.defaults"
    fi
    if ( cd "${out}/${ex}" && idf.py reconfigure >/dev/null \
         && SDKCONFIG_DEFAULTS="sdkconfig.defaults;managed_components/morsemicro__halow/configs/${MORSE_BOARD_PROFILE}" idf.py set-target esp32s3 >/dev/null \
         && idf.py build > build.log 2>&1 ); then
        grep -E "\.bin binary" "${out}/${ex}/build.log" || true
        echo "OK: ${ex}"
    else
        grep -E "error:" "${out}/${ex}/build.log" | head -5 || true
        echo "FAILED: ${ex} (see ${out}/${ex}/build.log)"
    fi
done
cat <<MSG

Generated under ${out}. Before flashing (Gate 4):
  - set the country code and SSID/passphrase in each project: idf.py menuconfig
    (Wi-Fi HaLow Connection Manager -> Country code / SSID / password; do not commit)
  - WM6180 RF port must be terminated (50 ohm) or in the attenuator chain; never open, never radiating in Japan
MSG
