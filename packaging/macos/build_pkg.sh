#!/usr/bin/env bash
# Builds an unsigned macOS installer (.pkg) for the Downslope plugin.
# Ships AU + VST3 only — no Standalone, no Host app.
#
# Prereq: a Release build exists:
#   cmake -B build -G Xcode
#   cmake --build build --config Release --target Downslope_AU Downslope_VST3
#
# Then:
#   packaging/macos/build_pkg.sh
#
# Output: packaging/macos/Downslope-<version>-macOS.pkg
# The .pkg installs to system /Library, so the installer prompts for admin.
# Unsigned for now — see packaging/README.md for where signing/notarization hooks go.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ARTEFACTS="${ROOT}/build/Downslope_artefacts/Release"
OUT_DIR="${ROOT}/packaging/macos"
STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT

VERSION="$(sed -n 's/.*project(Downslope VERSION \([0-9.]*\).*/\1/p' "${ROOT}/CMakeLists.txt")"
VERSION="${VERSION:-0.0.0}"

AU_ID="com.x09music.downslope.au"
VST3_ID="com.x09music.downslope.vst3"

AU_SRC="${ARTEFACTS}/AU/Downslope.component"
VST3_SRC="${ARTEFACTS}/VST3/Downslope.vst3"

for p in "$AU_SRC" "$VST3_SRC"; do
  if [[ ! -d "$p" ]]; then
    echo "error: missing $p" >&2
    echo "Build Release first: cmake --build build --config Release --target Downslope_AU Downslope_VST3" >&2
    exit 1
  fi
done

# Stage each format under its real install root.
AU_ROOT="${STAGE}/au"
VST3_ROOT="${STAGE}/vst3"
mkdir -p "${AU_ROOT}/Library/Audio/Plug-Ins/Components"
mkdir -p "${VST3_ROOT}/Library/Audio/Plug-Ins/VST3"
cp -R "$AU_SRC" "${AU_ROOT}/Library/Audio/Plug-Ins/Components/"
cp -R "$VST3_SRC" "${VST3_ROOT}/Library/Audio/Plug-Ins/VST3/"

PKG_DIR="${STAGE}/pkgs"
mkdir -p "$PKG_DIR"

pkgbuild --root "$AU_ROOT" \
  --identifier "$AU_ID" \
  --version "$VERSION" \
  --install-location / \
  "${PKG_DIR}/Downslope-AU.pkg"

pkgbuild --root "$VST3_ROOT" \
  --identifier "$VST3_ID" \
  --version "$VERSION" \
  --install-location / \
  "${PKG_DIR}/Downslope-VST3.pkg"

DIST="${STAGE}/distribution.xml"
cat > "$DIST" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>Downslope ${VERSION}</title>
    <options customize="allow" require-scripts="false" hostArchitectures="arm64,x86_64"/>
    <choices-outline>
        <line choice="au"/>
        <line choice="vst3"/>
    </choices-outline>
    <choice id="au" title="Audio Unit (AU)" description="Installs to /Library/Audio/Plug-Ins/Components">
        <pkg-ref id="${AU_ID}"/>
    </choice>
    <choice id="vst3" title="VST3" description="Installs to /Library/Audio/Plug-Ins/VST3">
        <pkg-ref id="${VST3_ID}"/>
    </choice>
    <pkg-ref id="${AU_ID}" version="${VERSION}">Downslope-AU.pkg</pkg-ref>
    <pkg-ref id="${VST3_ID}" version="${VERSION}">Downslope-VST3.pkg</pkg-ref>
</installer-gui-script>
EOF

mkdir -p "$OUT_DIR"
OUT_PKG="${OUT_DIR}/Downslope-${VERSION}-macOS.pkg"
productbuild --distribution "$DIST" --package-path "$PKG_DIR" "$OUT_PKG"

echo "Built: $OUT_PKG"
