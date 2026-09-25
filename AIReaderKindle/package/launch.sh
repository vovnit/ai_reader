#!/bin/sh
# Ran by `kpm launch aireader`, from the package folder.
cd "$(dirname "$0")"
export DISPLAY=:0
export AIREADER_KINDLE=1
export AIREADER_HOME=/mnt/us/aireader
export AIREADER_DATA_DIR="$(pwd)/share"
# Interface sizes follow the screen width; AIREADER_UI_SCALE=1.5 or
# AIREADER_UI_FONT="Sans 28px" in the environment override the guess.
mkdir -p "$AIREADER_HOME"
exec ./bin/aireader "$@" >> "$AIREADER_HOME/aireader.log" 2>&1
