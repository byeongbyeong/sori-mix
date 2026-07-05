#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_CONFIG="${BUILD_CONFIG:-Release}"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build}"
ARTEFACT_DIR="${BUILD_DIR}/SoriMix_artefacts/${BUILD_CONFIG}"
DIST_DIR="${DIST_DIR:-${ROOT_DIR}/dist}"
PACKAGE_SLUG="${PACKAGE_SLUG:-Sori1-macOS-preview}"
PACKAGE_DIR="${DIST_DIR}/${PACKAGE_SLUG}"
ZIP_PATH="${DIST_DIR}/${PACKAGE_SLUG}.zip"

AU_PATH="${ARTEFACT_DIR}/AU/SoriMix.component"
VST3_PATH="${ARTEFACT_DIR}/VST3/SoriMix.vst3"
APP_PATH="${ARTEFACT_DIR}/Standalone/SoriMix.app"

for required_path in "${AU_PATH}" "${VST3_PATH}" "${APP_PATH}"; do
  if [[ ! -e "${required_path}" ]]; then
    echo "Missing build artifact: ${required_path}" >&2
    echo "Build first, for example: cmake --build ${BUILD_DIR} --config ${BUILD_CONFIG}" >&2
    exit 1
  fi
done

rm -rf "${PACKAGE_DIR}" "${ZIP_PATH}"
mkdir -p \
  "${PACKAGE_DIR}/Plug-Ins/AU" \
  "${PACKAGE_DIR}/Plug-Ins/VST3" \
  "${PACKAGE_DIR}/Standalone"

ditto "${AU_PATH}" "${PACKAGE_DIR}/Plug-Ins/AU/SoriMix.component"
ditto "${VST3_PATH}" "${PACKAGE_DIR}/Plug-Ins/VST3/SoriMix.vst3"
ditto "${APP_PATH}" "${PACKAGE_DIR}/Standalone/SoriMix.app"

cat > "${PACKAGE_DIR}/README.txt" <<'README'
Sori 1 macOS Preview
====================

Thank you for trying Sori 1 by Sori LAB.

This preview package includes:

- AU: Plug-Ins/AU/SoriMix.component
- VST3: Plug-Ins/VST3/SoriMix.vst3
- Standalone app: Standalone/SoriMix.app
- One-click user install helper: Install Sori 1.command

Recommended install
-------------------

1. Unzip this package.
2. Double-click "Install Sori 1.command".
3. Restart your DAW or rescan plug-ins.
4. Look for the current build name "SoriMix" in your plug-in list.

Install locations
-----------------

The install helper copies files to:

- ~/Library/Audio/Plug-Ins/Components/SoriMix.component
- ~/Library/Audio/Plug-Ins/VST3/SoriMix.vst3
- ~/Applications/SoriMix.app

If macOS blocks the command file
-------------------------------

Right-click "Install Sori 1.command" and choose Open.

If your DAW blocks the plug-in
-----------------------------

This early preview is ad-hoc signed and not notarized yet. If macOS quarantine
blocks loading, remove quarantine after you trust this build:

  xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/SoriMix.component
  xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/SoriMix.vst3
  xattr -dr com.apple.quarantine ~/Applications/SoriMix.app

Commercial release note
-----------------------

The public product is Sori 1 by Sori LAB. The current internal JUCE target and
build artifact name is still SoriMix while the product is moving toward a
polished commercial release.
README

cat > "${PACKAGE_DIR}/Install Sori 1.command" <<'INSTALLER'
#!/usr/bin/env bash
set -euo pipefail

PACKAGE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

AU_SOURCE="${PACKAGE_DIR}/Plug-Ins/AU/SoriMix.component"
VST3_SOURCE="${PACKAGE_DIR}/Plug-Ins/VST3/SoriMix.vst3"
APP_SOURCE="${PACKAGE_DIR}/Standalone/SoriMix.app"

AU_DEST="${HOME}/Library/Audio/Plug-Ins/Components"
VST3_DEST="${HOME}/Library/Audio/Plug-Ins/VST3"
APP_DEST="${HOME}/Applications"

echo "Installing Sori 1 preview for the current user..."
echo

mkdir -p "${AU_DEST}" "${VST3_DEST}" "${APP_DEST}"

ditto "${AU_SOURCE}" "${AU_DEST}/SoriMix.component"
ditto "${VST3_SOURCE}" "${VST3_DEST}/SoriMix.vst3"
ditto "${APP_SOURCE}" "${APP_DEST}/SoriMix.app"

echo "Installed:"
echo "  ${AU_DEST}/SoriMix.component"
echo "  ${VST3_DEST}/SoriMix.vst3"
echo "  ${APP_DEST}/SoriMix.app"
echo
echo "Restart your DAW or rescan plug-ins. Current plug-in list name: SoriMix"
echo
read -r -p "Press Return to close this window."
INSTALLER

chmod +x "${PACKAGE_DIR}/Install Sori 1.command"

cat > "${PACKAGE_DIR}/VERSION.txt" <<VERSION
Package: ${PACKAGE_SLUG}
Build config: ${BUILD_CONFIG}
Generated: $(date -u +"%Y-%m-%dT%H:%M:%SZ")
VERSION

(
  cd "${DIST_DIR}"
  zip -qry -X "${ZIP_PATH}" "${PACKAGE_SLUG}"
)

echo "Created package: ${ZIP_PATH}"
