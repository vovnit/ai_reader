#!/bin/sh
# Builds the app for this machine, or hands off to the Kindle packaging.
#
#   ./build.sh                  desktop build into build/
#   ./build.sh check [book.epub [page.png]]
#                               desktop build, then the command-line check
#   ./build.sh run [args]       desktop build, then the app against the mock
#   ./build.sh kindle [target]  the .kpkg in Docker (kindlehf, or kindlepw2)
#
# Needs Meson, a C++17 compiler and GTK+ 2, Pango, SQLite, zlib and libcurl
# through pkg-config. On macOS: brew install gtk+ meson
set -e
root="$(cd "$(dirname "$0")" && pwd)"
builddir="$root/build"
# The bundled dictionary lives with the iOS app.
data="${AIREADER_DATA_DIR:-$root/../AIReader/AIReader/Resources}"

command="${1:-desktop}"
[ $# -gt 0 ] && shift

case "$command" in
    kindle)
        exec "$root/package/kindle-build.sh" "$@"
        ;;
    desktop|check|run)
        ;;
    *)
        echo "usage: $0 [check|run|kindle] [args]" >&2
        exit 1
        ;;
esac

[ -d "$builddir" ] || meson setup "$builddir"
meson compile -C "$builddir"

case "$command" in
    check)
        AIREADER_DATA_DIR="$data" exec "$builddir/aireader-check" "$@"
        ;;
    run)
        AIREADER_DATA_DIR="$data" exec "$builddir/aireader" --endpoint mock://ai --model mock-medium "$@"
        ;;
esac
