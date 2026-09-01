#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
firmware_dir="$(cd "${script_dir}/.." && pwd)"
output_dir="${firmware_dir}/porting_assistant"

# shellcheck disable=SC1091
source "${firmware_dir}/toolchain.lock"
"${script_dir}/check-toolchain.sh"

if [[ -e "${output_dir}" ]]; then
    echo "ERROR: ${output_dir} already exists; refusing to overwrite it." >&2
    exit 1
fi

cd "${firmware_dir}"
idf.py create-project-from-example \
    "${MORSE_HALOW_COMPONENT}=${MORSE_HALOW_VERSION}:porting_assistant"

cat <<EOF

Created ${output_dir}

Do not flash or run it until the RF safety gate in
docs/bringup/xiao-wm6180-first-boot.md is satisfied.

When that gate is satisfied:
  cd ${output_dir}
  idf.py reconfigure
  SDKCONFIG_DEFAULTS="sdkconfig.defaults;managed_components/morsemicro__halow/configs/${MORSE_BOARD_PROFILE}" idf.py set-target esp32s3
  idf.py menuconfig build
EOF
