#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DIST_DIR="${DIST_DIR:-${ROOT_DIR}/dist}"
PACKAGE_SLUG="${PACKAGE_SLUG:-Sori1-macOS-preview}"
PACKAGE_DIR="${DIST_DIR}/${PACKAGE_SLUG}"
ZIP_PATH="${DIST_DIR}/${PACKAGE_SLUG}.zip"

APPLE_ID_VALUE="${APPLE_NOTARIZATION_APPLE_ID:-${APPLE_ID:-}}"
APPLE_TEAM_VALUE="${APPLE_NOTARIZATION_TEAM_ID:-${APPLE_TEAM_ID:-}}"
APPLE_PASSWORD_VALUE="${APPLE_NOTARIZATION_PASSWORD:-${APPLE_APP_SPECIFIC_PASSWORD:-}}"

AU_PATH="${PACKAGE_DIR}/Plug-Ins/AU/SoriMix.component"
VST3_PATH="${PACKAGE_DIR}/Plug-Ins/VST3/SoriMix.vst3"
APP_PATH="${PACKAGE_DIR}/Standalone/SoriMix.app"

if [[ ! -f "${ZIP_PATH}" ]]; then
  echo "Missing package zip: ${ZIP_PATH}" >&2
  echo "Package first, for example: BUILD_CONFIG=Release ./scripts/package-mac.sh" >&2
  exit 1
fi

if [[ -z "${APPLE_ID_VALUE}" || -z "${APPLE_TEAM_VALUE}" || -z "${APPLE_PASSWORD_VALUE}" ]]; then
  echo "Apple notarization credentials are not configured. Skipping notarization."
  echo "Set APPLE_NOTARIZATION_APPLE_ID, APPLE_NOTARIZATION_TEAM_ID, and APPLE_NOTARIZATION_PASSWORD to notarize releases."
  exit 0
fi

echo "Submitting ${ZIP_PATH} to Apple notarization..."
xcrun notarytool submit "${ZIP_PATH}" \
  --apple-id "${APPLE_ID_VALUE}" \
  --team-id "${APPLE_TEAM_VALUE}" \
  --password "${APPLE_PASSWORD_VALUE}" \
  --wait

for bundle_path in "${AU_PATH}" "${VST3_PATH}" "${APP_PATH}"; do
  if [[ -e "${bundle_path}" ]]; then
    xcrun stapler staple "${bundle_path}"
    xcrun stapler validate "${bundle_path}"
  fi
done

(
  cd "${DIST_DIR}"
  rm -f "${ZIP_PATH}"
  zip -qry -X "${ZIP_PATH}" "${PACKAGE_SLUG}"
)

echo "Notarized and re-packaged: ${ZIP_PATH}"
