#!/bin/sh
# Cross-compiles the app and assembles a KPM package folder in dist/.
#
#   ./package/build.sh ~/x-tools/arm-kindlehf-linux-gnueabihf/meson-crosscompile.txt [target]
#
# Needs koxtoolchain and the Kindle SDK (see reference/kindle-dev); without
# them on this machine, package/kindle-build.sh does the same in Docker. Then:
#
#   python kpm-helper.py package pack dist/aireader dist
set -e
cross="$1"
target="${2:-kindle}"
if [ -z "$cross" ]; then
    echo "usage: $0 <meson-crosscompile.txt> [target]" >&2
    exit 1
fi
root="$(cd "$(dirname "$0")/.." && pwd)"
dictionary="$root/../AIReader/AIReader/Resources/dictionary.sqlite3"
out="$root/dist/aireader"

cd "$root"
builddir="build-$target"
[ -d "$builddir" ] || meson setup --cross-file "$cross" "$builddir"
meson compile -C "$builddir" aireader

rm -rf "$out"
mkdir -p "$out/bin" "$out/share" "$out/scriptlets"
cp "$builddir/aireader" "$out/bin/"
# Debug information is of no use on the device.
strip="$(sed -n "s/^strip = '\(.*\)'/\1/p" "$cross")"
[ -x "$strip" ] && "$strip" "$out/bin/aireader"
cp package/install.sh package/uninstall.sh package/launch.sh "$out/"
# One binary fits one platform; say which.
sed "s/\"supported_platforms\": .*/\"supported_platforms\": [\"$target\"]/" package/manifest.json > "$out/manifest.json"
cp package/scriptlets/aireader.sh "$out/scriptlets/"
chmod +x "$out"/*.sh "$out/scriptlets/aireader.sh" "$out/bin/aireader"
if [ -f "$dictionary" ]; then
    cp "$dictionary" "$out/share/dictionary.sqlite3"
else
    echo "warning: $dictionary not found; the package ships without the bundled dictionary" >&2
fi
echo "Package folder ready: $out"
