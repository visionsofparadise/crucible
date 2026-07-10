#!/usr/bin/env bash
# Crucible — build-time Linux tarball constructor.
#
# Invoked by the GitHub Actions release workflow after a successful Linux
# build. Assembles `Crucible FX.vst3`, `Crucible Instrument.vst3`, both
# Standalone binaries, and the user-facing install.sh into a flat (no
# top-level wrapper directory) tarball at
# dist/Crucible-${VERSION}-linux-x64.tar.gz.
#
# Usage: bash installers/linux/package.sh <VERSION>
set -euo pipefail

if [ "$#" -lt 1 ] || [ -z "${1:-}" ]; then
  echo "usage: $0 <VERSION>" >&2
  echo "       e.g. $0 v0.1.0" >&2
  exit 1
fi

VERSION="$1"

REPO_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
FX_BUILD_DIR="$REPO_ROOT/build/linux-release/FX/CrucibleFX_artefacts/Release"
INSTRUMENT_BUILD_DIR="$REPO_ROOT/build/linux-release/Instrument/CrucibleInstrument_artefacts/Release"
STAGE_DIR="$REPO_ROOT/build/linux-stage"
DIST_DIR="$REPO_ROOT/dist"
DEST="$DIST_DIR/Crucible-${VERSION}-linux-x64.tar.gz"

# Always clean up the staging tree, even if the run fails partway through.
# Leave dist/ alone — the workflow's upload step needs the tarball.
trap 'rm -rf "$STAGE_DIR"' EXIT

rm -rf "$STAGE_DIR"
mkdir -p "$STAGE_DIR"
mkdir -p "$DIST_DIR"

echo "Staging Linux tarball contents at $STAGE_DIR"
cp -R "$FX_BUILD_DIR/VST3/Crucible FX.vst3" "$STAGE_DIR/"
cp -R "$INSTRUMENT_BUILD_DIR/VST3/Crucible Instrument.vst3" "$STAGE_DIR/"
cp "$FX_BUILD_DIR/Standalone/Crucible FX" "$STAGE_DIR/"
cp "$INSTRUMENT_BUILD_DIR/Standalone/Crucible Instrument" "$STAGE_DIR/"
cp "$REPO_ROOT/installers/linux/install.sh" "$STAGE_DIR/"

# Defensive: the source executable bit on install.sh might not survive a
# Windows-side checkout, and the Standalone binary's bit depends on the
# build. Force both here so the resulting tarball is always usable.
chmod +x "$STAGE_DIR/Crucible FX"
chmod +x "$STAGE_DIR/Crucible Instrument"
chmod +x "$STAGE_DIR/install.sh"

echo "Building $DEST"
tar -czf "$DEST" -C "$STAGE_DIR" .

echo "Done: $DEST"
