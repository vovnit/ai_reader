#!/bin/sh
# The web app has no build step: the folder is the app. This serves it,
# checks it, and gathers it for hosting.
#
#   ./build.sh serve [port]     serves this folder at http://localhost:8080
#                               (open ?aiEndpoint=mock://ai to use the mock)
#   ./build.sh check [book.epub]
#                               everything below the views, under Node 22.12+;
#                               with AIREADER_SYNC_URL set, a WebDAV server too
#   ./build.sh site             dist/site, the files a web server serves —
#                               what Cloudflare deploys (wrangler.jsonc)
#   ./build.sh dist             the same as dist/AIReader-web.zip
set -e
root="$(cd "$(dirname "$0")" && pwd)"
files="index.html app.css manifest.webmanifest sw.js icon.svg icon-192.png icon-512.png dictionary.sqlite3 src"

site() {
    rm -rf "$root/dist/site"
    mkdir -p "$root/dist/site"
    # The dictionary is a link to the iOS app's copy; -L copies the file itself.
    (cd "$root" && cp -RL $files dist/site/)
}

case "${1:-serve}" in
    serve)
        exec python3 -m http.server "${2:-8080}" --bind 127.0.0.1 --directory "$root"
        ;;
    check)
        shift
        exec node "$root/tools/check.mjs" "$@"
        ;;
    site)
        site
        echo "dist/site"
        ;;
    dist)
        site
        rm -f "$root/dist/AIReader-web.zip"
        (cd "$root/dist/site" && zip -qr ../AIReader-web.zip .)
        echo "dist/AIReader-web.zip"
        ;;
    *)
        echo "usage: $0 [serve [port]|check [book.epub]|site|dist]" >&2
        exit 1
        ;;
esac
