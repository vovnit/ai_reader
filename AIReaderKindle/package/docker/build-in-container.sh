#!/bin/sh
# Ran inside the builder container by package/kindle-build.sh.
# Fetches the toolchain and generates the SDK once (both land in volumes),
# then cross-compiles and assembles the package folder.
set -e
target="$1"
case "$target" in
    kindlehf) tc=arm-kindlehf-linux-gnueabihf ;;
    kindlepw2) tc=arm-kindlepw2-linux-gnueabi ;;
    *) echo "unknown target: $target (kindlehf or kindlepw2)" >&2; exit 1 ;;
esac

if [ ! -x "/root/x-tools/$tc/bin/$tc-gcc" ]; then
    echo "[*] Downloading koxtoolchain for $target"
    curl -L -o /tmp/tc.tar.zst "https://github.com/koreader/koxtoolchain/releases/latest/download/$target.tar.zst"
    mkdir -p /root/x-tools
    # The tarball carries an x-tools/ folder of its own.
    tar --zstd -xf /tmp/tc.tar.zst -C /root/x-tools --strip-components=1
    rm /tmp/tc.tar.zst
fi

if [ ! -f "/root/x-tools/$tc/meson-crosscompile.txt" ]; then
    echo "[*] Generating the Kindle SDK for $target"
    [ -d /root/sdk/kindle-sdk ] || git clone --recursive --depth=1 https://github.com/KindleModding/kindle-sdk.git /root/sdk/kindle-sdk
    cd /root/sdk/kindle-sdk
    ./gen-sdk.sh "$target"
fi

cd /work/AIReaderKindle
exec ./package/build.sh "/root/x-tools/$tc/meson-crosscompile.txt" "$target"
