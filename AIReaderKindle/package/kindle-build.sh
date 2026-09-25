#!/bin/sh
# Builds the Kindle package without installing a toolchain on this machine:
# a Docker container holds koxtoolchain and the Kindle SDK, kept in volumes
# between runs. The package folder and the installable .kpkg land in dist/.
#
#   ./package/kindle-build.sh [kindlehf|kindlepw2]
#
# Packing uses KPM's own helper, which wants Python 3.12 or later.
set -e
target="${1:-kindlehf}"
root="$(cd "$(dirname "$0")/.." && pwd)"
docker build --platform linux/amd64 -t aireader-kindle-builder "$root/package/docker"
docker run --rm --privileged --platform linux/amd64 \
    -v aireader-kindle-tools:/root/x-tools \
    -v aireader-kindle-sdk:/root/sdk \
    -v "$root/..:/work" \
    aireader-kindle-builder \
    /work/AIReaderKindle/package/docker/build-in-container.sh "$target"

helper="$root/dist/kpm-helper.py"
[ -f "$helper" ] || curl -sL -o "$helper" https://raw.githubusercontent.com/KindleModding/KPM/main/kpm-helper.py
python3 "$helper" package pack "$root/dist/aireader" "$root/dist"
