#! /bin/bash

# make sure that we can access the FS-UAE directory using a path relative to run.sh
# no matter what the current working directory is
parent_dir=$( cd "$(dirname "${BASH_SOURCE[0]}")" ; pwd -P )
cd "$parent_dir"

$HOME/Applications/Emulators/Amiga/FS-UAE.app/Contents/MacOS/fs-uae ../FS-UAE\ Configurations/A4000\ -\ Apollo\ OS.fs-uae
