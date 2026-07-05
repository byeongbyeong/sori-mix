#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DIST_DIR="${DIST_DIR:-${ROOT_DIR}/dist}"
PACKAGE_SLUG="${PACKAGE_SLUG:-Sori1-macOS-preview}"
PKG_PATH="${DIST_DIR}/${PACKAGE_SLUG}.pkg"

APPLE_ID_VALUE="${APPLE_NOTARIZATION_APPLE_ID:-${APPLE_ID:-}}"
APPLE_TEAM_VALUE="${APPLE_NOTARIZATION_TEAM_ID:-${APPLE_TEAM_ID:-}}"
APPLE_PASSWORD_VALUE="${APPLE_NOTARIZATION_PASSWORD:-${APPLE_APP_SPECIFIC_PASSWORD:-}}"

if [[ ! -f "${PKG_PATH}" ]]; then
  echo "Missing installer package: ${PKG_PATH}" >&2
  echo "Build it first: ./scripts/build-mac-pkg.sh" >&2
  exit 1
fi

if ! pkgutil --check-signature "${PKG_PATH}" >/dev/null 2>&1; then
  echo "Installer package is unsigned. Skipping pkg notarization."
  echo "Set MACOS_INSTALLER_CERTIFICATE_P12_BASE64 and MACOS_INSTALLER_CERTIFICATE_PASSWORD to sign installer packages."
  exit 0
fi

if [[ -z "${APPLE_ID_VALUE}" || -z "${APPLE_TEAM_VALUE}" || -z "${APPLE_PASSWORD_VALUE}" ]]; then
  echo "Apple notarization credentials are not configured. Skipping pkg notarization."
  echo "Set APPLE_NOTARIZATION_APPLE_ID, APPLE_NOTARIZATION_TEAM_ID, and APPLE_NOTARIZATION_PASSWORD to notarize releases."
  exit 0
fi

echo "Submitting ${PKG_PATH} to Apple notarization..."
xcrun notarytool submit "${PKG_PATH}" \
  --apple-id "${APPLE_ID_VALUE}" \
  --team-id "${APPLE_TEAM_VALUE}" \
  --password "${APPLE_PASSWORD_VALUE}" \
  --wait

xcrun stapler staple "${PKG_PATH}"
xcrun stapler validate "${PKG_PATH}"

echo "Notarized installer package: ${PKG_PATH}"
