#!/usr/bin/env bash
# Crucible — user-facing Linux installer.
#
# Ships inside Crucible-vX.Y.Z-linux-x64.tar.gz alongside the two VST3
# bundles and Standalone executables. The end user extracts the tarball
# into a fresh directory and runs `bash install.sh`. Installs to
# user-local paths only; never uses sudo, never writes to system
# locations.
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

FX_VST3_SRC="$SCRIPT_DIR/Crucible FX.vst3"
INSTRUMENT_VST3_SRC="$SCRIPT_DIR/Crucible Instrument.vst3"
FX_STANDALONE_SRC="$SCRIPT_DIR/Crucible FX"
INSTRUMENT_STANDALONE_SRC="$SCRIPT_DIR/Crucible Instrument"

for src in "$FX_VST3_SRC" "$INSTRUMENT_VST3_SRC"; do
  if [ ! -d "$src" ]; then
    echo "error: '$src' not found next to install.sh" >&2
    echo "       Extract the full tarball into a single directory before running this script." >&2
    exit 1
  fi
done

for src in "$FX_STANDALONE_SRC" "$INSTRUMENT_STANDALONE_SRC"; do
  if [ ! -f "$src" ]; then
    echo "error: '$src' not found next to install.sh" >&2
    echo "       Extract the full tarball into a single directory before running this script." >&2
    exit 1
  fi
done

VST3_DEST="$HOME/.vst3"
BIN_DEST="$HOME/.local/bin"
DOC_DEST="$HOME/.local/share/doc/crucible"

mkdir -p "$VST3_DEST" "$BIN_DEST" "$DOC_DEST"

cp -R "$FX_VST3_SRC" "$VST3_DEST/"
cp -R "$INSTRUMENT_VST3_SRC" "$VST3_DEST/"
cp "$FX_STANDALONE_SRC" "$BIN_DEST/crucible-fx"
cp "$INSTRUMENT_STANDALONE_SRC" "$BIN_DEST/crucible-instrument"
chmod +x "$BIN_DEST/crucible-fx"
chmod +x "$BIN_DEST/crucible-instrument"
cp "$SCRIPT_DIR/LICENSE" "$SCRIPT_DIR/THIRD-PARTY-NOTICES.txt" "$DOC_DEST/"

echo "Crucible installed:"
echo "  VST3 (FX):         $VST3_DEST/Crucible FX.vst3"
echo "  VST3 (Instrument): $VST3_DEST/Crucible Instrument.vst3"
echo "  Standalone (FX):         $BIN_DEST/crucible-fx"
echo "  Standalone (Instrument): $BIN_DEST/crucible-instrument"
echo "  Licence and notices:     $DOC_DEST"
echo
echo "Note: '$BIN_DEST' may need to be on your PATH for the standalones to run from any shell."
