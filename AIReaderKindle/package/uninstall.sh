#!/bin/sh
# Ran by KPM before the package folder is deleted (with "upgrade" during an upgrade).
# Only the scriptlet is removed, and only if it is still ours. The reader's
# books, dictionaries and lookups under /mnt/us/aireader are left alone.
if [ -f /mnt/us/documents/aireader.sh ]; then
    if [ "$(md5sum < /mnt/us/documents/aireader.sh)" = "$(md5sum < ./scriptlets/aireader.sh)" ]; then
        rm /mnt/us/documents/aireader.sh
    fi
fi
