#!/bin/bash
#
# MzDebirth for macOS: put everything where it belongs, for this user, without an installer.
#
# Run it through bash rather than double-clicking it:
#
#     bash ~/Downloads/MzDebirth-*-macOS-beta/install.sh
#
# (type "bash ", drag this file into the Terminal window, press Return). Going through bash
# is what lets it run at all: macOS refuses to open a downloaded script or app that has not
# been through Apple's notarization, and this project does not go through Apple.
#
# What it does - nothing needs an administrator:
#
#   MzDebirth.app   ->  /Applications, or ~/Applications if that is not writable
#   Disks/*.hfe     ->  ~/Library/Application Support/MzDebirth/Disks  (never over your own)
#
# and it takes the "downloaded from the internet" quarantine flag off each, which is the one
# thing standing between an unnotarized app and its opening.
#
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"

disk_dir="$HOME/Library/Application Support/MzDebirth/Disks"
if [ -w /Applications ]; then app_dir="/Applications"; else app_dir="$HOME/Applications"; fi

mkdir -p "$disk_dir" "$app_dir"

echo "Installing MzDebirth"

# The app, the old one out first so nothing stale survives inside it.
rm -rf "$app_dir/MzDebirth.app"
ditto "$here/MzDebirth.app" "$app_dir/MzDebirth.app"
xattr -dr com.apple.quarantine "$app_dir/MzDebirth.app" 2>/dev/null || true
echo "  MzDebirth.app  ->  $app_dir"

# The Mz950's disks, copied only where a disk of that name is not already there: they are
# yours to edit, and an update should not undo the edits.
copied=0
for disk in "$here"/Disks/*.hfe; do
    [ -e "$disk" ] || continue
    name="$(basename "$disk")"
    if [ ! -e "$disk_dir/$name" ]; then
        cp "$disk" "$disk_dir/$name"
        copied=$((copied + 1))
    fi
done
xattr -dr com.apple.quarantine "$disk_dir" 2>/dev/null || true
echo "  $copied new disk(s)  ->  $disk_dir"

echo
echo "Done. Open MzDebirth from $app_dir."
