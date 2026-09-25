#!/bin/sh
# This script handles launching the package
# Running "kpm launch example_package" will cause this script to run
# This script is useful for launchers and it is recommended that scriptlets use the `kpm launch` mechanism as to not worry about installation path.

echo "Launching example package..."

# The following environment variables are present as of KPM 0.3.0
echo "Running KPM $KPM_VERSION_MAJOR.$KPM_VERSION_MINOR.$KPM_VERSION_PATCH on $KPM_PLATFORM"

echo "$1" # Will output "hello" when launched via the scriptlet in scriptlets/example_kpm.sh

cat ./manifest.json # Like installation and uninstallation scripts, the launch script runs in the unpacked package folder