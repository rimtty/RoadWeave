#!/usr/bin/env bash
# Fetch the pinned libopus source into ./upstream (not committed; see .gitignore).
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
OPUS_TAG="v1.5.2"
if [[ -d upstream ]]; then
    echo "upstream already exists ($(git -C upstream describe --tags 2>/dev/null || echo unknown))"
    exit 0
fi
git clone --depth 1 -b "${OPUS_TAG}" https://github.com/xiph/opus.git upstream
echo "fetched opus ${OPUS_TAG}"
