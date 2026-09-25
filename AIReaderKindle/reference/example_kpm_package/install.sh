#!/bin/sh

# This script will be run when the package is installed
# As of KPM 0.3.0 it will be called with a parameter of "upgrade" if this installation is part of an upgrade
# ie: `install.sh upgrade`

if [ "$1" = "upgrade" ]; then
    echo "Run as upgrade!"
fi

echo "Hello from example package!" # stdout is shown in the KPM log

# The following environment variables are present as of KPM 0.3.0
echo "Running KPM $KPM_VERSION_MAJOR.$KPM_VERSION_MINOR.$KPM_VERSION_PATCH on $KPM_PLATFORM"

cat ./manifest.json # The script runs relative to the unpacked package directory, so local files can be accessed as such

cp scriptlets/example_kpm.sh /mnt/us/documents/ # Packages should handle their own scriptlet installation themselves

# The script should use exit code 0 to indicate success, anything else will result in failure