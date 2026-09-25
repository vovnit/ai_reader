#!/bin/sh
# Ran by KPM after the package is unpacked, from the package folder.
# Books, dictionaries, settings and past lookups live on the USB-visible
# partition so they can be copied in from a computer and survive updates.
mkdir -p /mnt/us/aireader/books /mnt/us/aireader/dictionaries
chmod +x ./launch.sh ./bin/aireader
cp scriptlets/aireader.sh /mnt/us/documents/
echo "AIReader installed. Copy .epub files to /mnt/us/aireader/books and open AIReader from the library."
