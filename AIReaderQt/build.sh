#!/bin/sh
# Builds the Qt app for Linux in a container, so nothing needs installing
# here but Docker.
#
#   ./build.sh                  build into build-docker/
#   ./build.sh script <file>    build, then run the smoke script offscreen
#                               against the mock, in a fresh home folder
#   ./build.sh appimage         dist/AIReader-x86_64.AppImage, built on
#                               Ubuntu 22.04 for x86_64
#   ./build.sh shell            a shell in the container
#
# On a Linux machine with Qt 6, pango, gdk-pixbuf, SQLite, zlib and libcurl
# installed, plain CMake does the same:
#   cmake -B build -G Ninja && cmake --build build
set -e
root="$(cd "$(dirname "$0")/.." && pwd)"
image=aireader-qt-builder
docker build -q -t "$image" "$root/AIReaderQt/docker" >/dev/null
run() {
    docker run --rm -v "$root:/work" -w /work/AIReaderQt "$image" sh -c "$1"
}
build='cmake -S . -B build-docker -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo >/dev/null && cmake --build build-docker'

case "${1:-build}" in
    build)
        run "$build"
        ;;
    script)
        run "$build && rm -rf /tmp/home && mkdir -p /tmp/home/books && cp ${3:-/dev/null} /tmp/home/books/ 2>/dev/null; \
             AIREADER_HOME=/tmp/home AIREADER_DATA_DIR=/work/AIReader/AIReader/Resources AIREADER_SCRIPT=$2 \
             ./build-docker/aireader-qt --endpoint mock://ai --model mock-medium"
        ;;
    appimage)
        docker build -q --platform linux/amd64 -t aireader-qt-appimage \
            -f "$root/AIReaderQt/docker/Dockerfile.appimage" "$root/AIReaderQt/docker" >/dev/null
        docker run --rm --platform linux/amd64 -v "$root:/work" -w /work/AIReaderQt aireader-qt-appimage docker/appimage.sh
        ;;
    shell)
        docker run --rm -it -v "$root:/work" -w /work/AIReaderQt "$image" bash
        ;;
esac
