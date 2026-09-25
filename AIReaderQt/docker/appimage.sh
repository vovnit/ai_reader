#!/bin/sh
# Run inside the AppImage container: builds the app, installs it into an
# AppDir, and has linuxdeploy bundle its libraries and Qt plugins.
set -e
build=build-appimage
cmake -S . -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr >/dev/null
cmake --build "$build"
rm -rf "$build/AppDir"
DESTDIR="$build/AppDir" cmake --install "$build" >/dev/null

# The decoders gdk-pixbuf does not have built in (GIF, TIFF…), with a cache
# listing them for the app to fill in with where it is mounted.
pixbuf=/usr/lib/x86_64-linux-gnu/gdk-pixbuf-2.0
loaders="$build/AppDir/usr/lib/gdk-pixbuf-2.0/2.10.0"
mkdir -p "$loaders/loaders"
cp "$pixbuf"/2.10.0/loaders/*.so "$loaders/loaders/"
"$pixbuf/gdk-pixbuf-query-loaders" "$loaders"/loaders/*.so \
    | sed "s|\"[^\"]*/loaders/|\"@LOADERS@/|" > "$loaders/loaders.cache.in"

# The offscreen platform too, so the AppImage can be run headless.
export EXTRA_PLATFORM_PLUGINS=libqoffscreen.so
export LDAI_OUTPUT=AIReader-x86_64.AppImage OUTPUT=AIReader-x86_64.AppImage
mkdir -p dist
cd dist
# linuxdeploy leaves fribidi and HarfBuzz to the host, as it does the X and
# GL libraries. Both go with the Pango built against them instead: a minimal
# system may have no fribidi, and the shaper tested with Pango is the one
# shipped.
linuxdeploy --appdir "../$build/AppDir" --plugin qt --output appimage \
    --library /usr/lib/x86_64-linux-gnu/libfribidi.so.0 \
    --library /usr/lib/x86_64-linux-gnu/libharfbuzz.so.0 \
    --deploy-deps-only "$(pwd)/../$loaders/loaders" \
    --desktop-file ../packaging/aireader.desktop --icon-file ../packaging/aireader.svg
