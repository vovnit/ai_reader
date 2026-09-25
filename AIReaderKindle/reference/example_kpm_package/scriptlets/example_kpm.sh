#!/bin/sh

echo "Hello from the example package scriptlet!"

# KPM will always be accessible from /var/local/kmc/bin/kpm
# It is recommended to use this method to launch packages
/var/local/kmc/bin/kpm launch example_package hello # Additional parameters (ie: "hello") are passed through to the launch script