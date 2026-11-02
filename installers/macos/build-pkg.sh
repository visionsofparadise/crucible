#!/usr/bin/env bash
# Build a macOS installer .dmg containing a multi-component .pkg that installs
# both Crucible variants. VST3 bundles go to /Library/Audio/Plug-Ins/VST3/ and
# Standalone .apps go to /Applications/. Unsigned for the free v1 — Gatekeeper
# unblock is documented in the README.
#
# Usage: build-pkg.sh <VERSION>     (e.g. build-pkg.sh v0.1.0)
#
# Output: $REPO_ROOT/dist/Crucible-<VERSION>-macos-universal.dmg
#
# DMG tool: hdiutil (built into every macOS host — no Homebrew dep needed).

set -euo pipefail

if [[ $# -lt 1 || -z "${1:-}" ]]; then
  echo "Usage: $0 <VERSION>" >&2
  echo "  e.g. $0 v0.1.0" >&2
  exit 1
fi

VERSION="$1"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
FX_BUILD_DIR="${REPO_ROOT}/build/macos-release/FX/CrucibleFX_artefacts/Release"
INSTRUMENT_BUILD_DIR="${REPO_ROOT}/build/macos-release/Instrument/CrucibleInstrument_artefacts/Release"

PAYLOAD_VST3="${REPO_ROOT}/build/pkg-payload-vst3"
PAYLOAD_APP="${REPO_ROOT}/build/pkg-payload-app"
COMPONENTS_DIR="${REPO_ROOT}/build/pkg-components"
DISTRIBUTION_XML="${REPO_ROOT}/build/distribution.xml"
PRODUCT_PKG="${REPO_ROOT}/build/Crucible.pkg"
DIST_DIR="${REPO_ROOT}/dist"
DMG_PATH="${DIST_DIR}/Crucible-${VERSION}-macos-universal.dmg"
DMG_STAGING="${REPO_ROOT}/build/dmg-staging"

cleanup() {
  rm -rf \
    "${PAYLOAD_VST3}" \
    "${PAYLOAD_APP}" \
    "${COMPONENTS_DIR}" \
    "${DISTRIBUTION_XML}" \
    "${PRODUCT_PKG}" \
    "${DMG_STAGING}"
}
trap cleanup EXIT

echo "==> Building Crucible installer ${VERSION}"
echo "    REPO_ROOT=${REPO_ROOT}"
echo "    FX_BUILD_DIR=${FX_BUILD_DIR}"
echo "    INSTRUMENT_BUILD_DIR=${INSTRUMENT_BUILD_DIR}"

# --- Stage VST3 payload (both variants) ----------------------------------
echo "==> Staging VST3 payload"
rm -rf "${PAYLOAD_VST3}"
mkdir -p "${PAYLOAD_VST3}/Library/Audio/Plug-Ins/VST3"
cp -R "${FX_BUILD_DIR}/VST3/Crucible FX.vst3" \
  "${PAYLOAD_VST3}/Library/Audio/Plug-Ins/VST3/Crucible FX.vst3"
cp -R "${INSTRUMENT_BUILD_DIR}/VST3/Crucible Instrument.vst3" \
  "${PAYLOAD_VST3}/Library/Audio/Plug-Ins/VST3/Crucible Instrument.vst3"

# Licence and third-party notices install beside the plugins rather than
# inside the bundles, whose ad-hoc signatures seal their contents.
mkdir -p "${PAYLOAD_VST3}/Library/Application Support/ZCROSS/Crucible"
cp "${REPO_ROOT}/LICENSE" "${REPO_ROOT}/THIRD-PARTY-NOTICES.txt" \
  "${PAYLOAD_VST3}/Library/Application Support/ZCROSS/Crucible/"

# --- Stage Standalone .app payload (both variants) -----------------------
echo "==> Staging Standalone .app payload"
rm -rf "${PAYLOAD_APP}"
mkdir -p "${PAYLOAD_APP}/Applications"
cp -R "${FX_BUILD_DIR}/Standalone/Crucible FX.app" \
  "${PAYLOAD_APP}/Applications/Crucible FX.app"
cp -R "${INSTRUMENT_BUILD_DIR}/Standalone/Crucible Instrument.app" \
  "${PAYLOAD_APP}/Applications/Crucible Instrument.app"

# --- Build component pkgs ------------------------------------------------
echo "==> Building component pkgs"
rm -rf "${COMPONENTS_DIR}"
mkdir -p "${COMPONENTS_DIR}"

pkgbuild \
  --root "${PAYLOAD_VST3}" \
  --identifier media.zcross.crucible.vst3 \
  --version "${VERSION}" \
  --install-location / \
  "${COMPONENTS_DIR}/CrucibleVST3.pkg"

pkgbuild \
  --root "${PAYLOAD_APP}" \
  --identifier media.zcross.crucible.standalone \
  --version "${VERSION}" \
  --install-location / \
  "${COMPONENTS_DIR}/CrucibleStandalone.pkg"

# --- Generate distribution.xml -------------------------------------------
echo "==> Writing distribution.xml"
cat > "${DISTRIBUTION_XML}" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>Crucible</title>
    <options customize="never" require-scripts="false" hostArchitectures="x86_64,arm64"/>
    <choices-outline>
        <line choice="media.zcross.crucible.vst3"/>
        <line choice="media.zcross.crucible.standalone"/>
    </choices-outline>
    <choice id="media.zcross.crucible.vst3" title="Crucible VST3 (FX + Instrument)" visible="true">
        <pkg-ref id="media.zcross.crucible.vst3"/>
    </choice>
    <choice id="media.zcross.crucible.standalone" title="Crucible Standalone (FX + Instrument)" visible="true">
        <pkg-ref id="media.zcross.crucible.standalone"/>
    </choice>
    <pkg-ref id="media.zcross.crucible.vst3">CrucibleVST3.pkg</pkg-ref>
    <pkg-ref id="media.zcross.crucible.standalone">CrucibleStandalone.pkg</pkg-ref>
</installer-gui-script>
EOF

# --- Build product pkg ---------------------------------------------------
echo "==> Building product pkg"
productbuild \
  --distribution "${DISTRIBUTION_XML}" \
  --package-path "${COMPONENTS_DIR}" \
  --version "${VERSION}" \
  "${PRODUCT_PKG}"

# --- Build .dmg ----------------------------------------------------------
echo "==> Building .dmg at ${DMG_PATH}"
mkdir -p "${DIST_DIR}"
rm -rf "${DMG_STAGING}"
mkdir -p "${DMG_STAGING}"
cp "${PRODUCT_PKG}" "${DMG_STAGING}/Crucible.pkg"
cp "${REPO_ROOT}/LICENSE" "${REPO_ROOT}/THIRD-PARTY-NOTICES.txt" "${DMG_STAGING}/"

# Overwrite any existing dmg of the same name; UDZO = compressed read-only.
hdiutil create \
  -volname "Crucible" \
  -srcfolder "${DMG_STAGING}" \
  -ov \
  -format UDZO \
  "${DMG_PATH}"

echo "==> Done: ${DMG_PATH}"
