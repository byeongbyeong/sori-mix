#!/usr/bin/env bash
set -euo pipefail
export COPYFILE_DISABLE=1

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DIST_DIR="${DIST_DIR:-${ROOT_DIR}/dist}"
PACKAGE_SLUG="${PACKAGE_SLUG:-Sori1-macOS-preview}"
PACKAGE_DIR="${DIST_DIR}/${PACKAGE_SLUG}"
PKG_WORK_DIR="${DIST_DIR}/pkg-work"
PKG_ROOT="${PKG_WORK_DIR}/root"
PKG_COMPONENT="${PKG_WORK_DIR}/${PACKAGE_SLUG}-component.pkg"
PKG_PATH="${DIST_DIR}/${PACKAGE_SLUG}.pkg"

PACKAGE_VERSION="${PACKAGE_VERSION:-$(sed -n 's/^project(SoriMix VERSION \([^ ]*\).*/\1/p' "${ROOT_DIR}/CMakeLists.txt")}"
PACKAGE_VERSION="${PACKAGE_VERSION:-0.1.0}"
PKG_IDENTIFIER="${PKG_IDENTIFIER:-com.sorilab.sori1.pkg}"
INSTALLER_SIGN_IDENTITY="${MACOS_INSTALLER_SIGN_IDENTITY:-}"
TEMP_KEYCHAIN=""

AU_SOURCE="${PACKAGE_DIR}/Plug-Ins/AU/SoriMix.component"
VST3_SOURCE="${PACKAGE_DIR}/Plug-Ins/VST3/SoriMix.vst3"
APP_SOURCE="${PACKAGE_DIR}/Standalone/SoriMix.app"

for required_path in "${AU_SOURCE}" "${VST3_SOURCE}" "${APP_SOURCE}"; do
  if [[ ! -e "${required_path}" ]]; then
    echo "Missing package payload: ${required_path}" >&2
    echo "Run package-mac.sh first, for example: BUILD_CONFIG=Release ./scripts/package-mac.sh" >&2
    exit 1
  fi
done

cleanup() {
  if [[ -n "${TEMP_KEYCHAIN}" ]]; then
    security delete-keychain "${TEMP_KEYCHAIN}" >/dev/null 2>&1 || true
  fi
}
trap cleanup EXIT

decode_base64_file() {
  local encoded_path="$1"
  local output_path="$2"

  if base64 --decode "${encoded_path}" > "${output_path}" 2>/dev/null; then
    return 0
  fi

  base64 -D -i "${encoded_path}" -o "${output_path}"
}

if [[ -n "${MACOS_INSTALLER_CERTIFICATE_P12_BASE64:-}" ]]; then
  if [[ -z "${MACOS_INSTALLER_CERTIFICATE_PASSWORD:-}" ]]; then
    echo "MACOS_INSTALLER_CERTIFICATE_PASSWORD is required when MACOS_INSTALLER_CERTIFICATE_P12_BASE64 is set." >&2
    exit 1
  fi

  TEMP_KEYCHAIN="${RUNNER_TEMP:-/tmp}/sori1-installer-signing.keychain-db"
  KEYCHAIN_PASSWORD="${MACOS_KEYCHAIN_PASSWORD:-$(uuidgen)}"
  CERT_B64_PATH="${RUNNER_TEMP:-/tmp}/sori1-installer-certificate.p12.base64"
  CERT_PATH="${RUNNER_TEMP:-/tmp}/sori1-installer-certificate.p12"

  printf "%s" "${MACOS_INSTALLER_CERTIFICATE_P12_BASE64}" > "${CERT_B64_PATH}"
  decode_base64_file "${CERT_B64_PATH}" "${CERT_PATH}"

  security create-keychain -p "${KEYCHAIN_PASSWORD}" "${TEMP_KEYCHAIN}"
  security set-keychain-settings -lut 21600 "${TEMP_KEYCHAIN}"
  security unlock-keychain -p "${KEYCHAIN_PASSWORD}" "${TEMP_KEYCHAIN}"
  security import "${CERT_PATH}" -P "${MACOS_INSTALLER_CERTIFICATE_PASSWORD}" -A -t cert -f pkcs12 -k "${TEMP_KEYCHAIN}"
  security list-keychains -d user -s "${TEMP_KEYCHAIN}" $(security list-keychains -d user | sed 's/[ "]//g')

  if [[ -z "${INSTALLER_SIGN_IDENTITY}" ]]; then
    INSTALLER_SIGN_IDENTITY="$(security find-identity -v -p basic "${TEMP_KEYCHAIN}" | awk -F '"' '/Developer ID Installer/ {print $2; exit}')"
  fi
fi

rm -rf "${PKG_WORK_DIR}" "${PKG_PATH}"
mkdir -p \
  "${PKG_ROOT}/Library/Audio/Plug-Ins/Components" \
  "${PKG_ROOT}/Library/Audio/Plug-Ins/VST3" \
  "${PKG_ROOT}/Applications"

ditto --norsrc "${AU_SOURCE}" "${PKG_ROOT}/Library/Audio/Plug-Ins/Components/SoriMix.component"
ditto --norsrc "${VST3_SOURCE}" "${PKG_ROOT}/Library/Audio/Plug-Ins/VST3/SoriMix.vst3"
ditto --norsrc "${APP_SOURCE}" "${PKG_ROOT}/Applications/SoriMix.app"
xattr -cr "${PKG_ROOT}" || true
dot_clean -m "${PKG_ROOT}" || true
find "${PKG_ROOT}" \( -name ".DS_Store" -o -name "._*" \) -delete

pkgbuild \
  --root "${PKG_ROOT}" \
  --identifier "${PKG_IDENTIFIER}" \
  --version "${PACKAGE_VERSION}" \
  --install-location "/" \
  --ownership recommended \
  --filter '(^|/)\._.*' \
  --filter '(^|/)\.DS_Store$' \
  "${PKG_COMPONENT}"

if [[ -n "${INSTALLER_SIGN_IDENTITY}" ]]; then
  echo "Signing installer package with identity: ${INSTALLER_SIGN_IDENTITY}"
  productbuild --sign "${INSTALLER_SIGN_IDENTITY}" --package "${PKG_COMPONENT}" "${PKG_PATH}"
  pkgutil --check-signature "${PKG_PATH}"
else
  echo "No Developer ID Installer certificate configured. Creating unsigned preview installer package."
  productbuild --package "${PKG_COMPONENT}" "${PKG_PATH}"
fi

pkgutil --payload-files "${PKG_PATH}" >/dev/null
echo "Created installer package: ${PKG_PATH}"
