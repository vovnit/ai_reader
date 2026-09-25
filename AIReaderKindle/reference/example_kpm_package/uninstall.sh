#!/bin/sh

# This script will be run when the package is uninstalled
# As of KPM 0.1.0 it will be called with a parameter of "upgrade" if this installation is part of an upgrade
# ie: `uninstall.sh upgrade`

if [ "$1" = "upgrade" ]; then
    echo "Run as upgrade!"
fi

# Works the same as install.sh
echo "Hello from example package!" # stdout is shown in the KPM log

# The following environment variables are present as of KPM 0.3.0
echo "Running KPM $KPM_VERSION_MAJOR.$KPM_VERSION_MINOR.$KPM_VERSION_PATCH on $KPM_PLATFORM"

cat ./manifest.json # The script runs relative to the unpacked package directory, so local files can be accessed as such

# Packages should ensure that they do not delete a scriptlet that isn't their own
if [ -f "/mnt/us/documents/example_kpm.sh" ]; then
    if [ "$(md5sum /mnt/us/documents/example_kpm.sh)" = "$(md5sum ./scriptlets/example_kpm.sh)" ]; then
        rm /mnt/us/documents/example_kpm.sh # Packages should remove their installed scriptlet themselves (if applicable)
    fi
fi

# Other files dropped by the package should be removed here

###
# NOTE
###
# You should not delete files in the package folder (./) via the uninstall script
# That is handled by KPM automatically when uninstallation is succesful

# The script should use exit code 0 to indicate success, anything else will result in failure