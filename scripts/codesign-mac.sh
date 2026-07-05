#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_CONFIG="${BUILD_CONFIG:-Release}"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build}"
ARTEFACT_DIR="${BUILD_DIR}/SoriMix_artefacts/${BUILD_CONFIG}"

AU_PATH="${ARTEFACT_DIR}/AU/SoriMix.component"
VST3_PATH="${ARTEFACT_DIR}/VST3/SoriMix.vst3"
APP_PATH="${ARTEFACT_DIR}/Standalone/SoriMix.app"

SIGN_IDENTITY="${MACOS_CODESIGN_IDENTITY:-}"
TEMP_KEYCHAIN=""

for required_path in "${AU_PATH}" "${VST3_PATH}" "${APP_PATH}"; do
  if [[ ! -e "${required_path}" ]]; then
    echo "Missing build artifact: ${required_path}" >&2
    echo "Build first, for example: cmake --build ${BUILD_DIR} --config ${BUILD_CONFIG}" >&2
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

if [[ -n "${MACOS_CERTIFICATE_P12_BASE64:-}" ]]; then
  if [[ -z "${MACOS_CERTIFICATE_PASSWORD:-}" ]]; then
    echo "MACOS_CERTIFICATE_PASSWORD is required when MACOS_CERTIFICATE_P12_BASE64 is set." >&2
    exit 1
  fi

  TEMP_KEYCHAIN="${RUNNER_TEMP:-/tmp}/sori1-signing.keychain-db"
  KEYCHAIN_PASSWORD="${MACOS_KEYCHAIN_PASSWORD:-$(uuidgen)}"
  CERT_B64_PATH="${RUNNER_TEMP:-/tmp}/sori1-certificate.p12.base64"
  CERT_PATH="${RUNNER_TEMP:-/tmp}/sori1-certificate.p12"

  printf "%s" "${MACOS_CERTIFICATE_P12_BASE64}" > "${CERT_B64_PATH}"
  decode_base64_file "${CERT_B64_PATH}" "${CERT_PATH}"

  security create-keychain -p "${KEYCHAIN_PASSWORD}" "${TEMP_KEYCHAIN}"
  security set-keychain-settings -lut 21600 "${TEMP_KEYCHAIN}"
  security unlock-keychain -p "${KEYCHAIN_PASSWORD}" "${TEMP_KEYCHAIN}"
  security import "${CERT_PATH}" -P "${MACOS_CERTIFICATE_PASSWORD}" -A -t cert -f pkcs12 -k "${TEMP_KEYCHAIN}"
  security set-key-partition-list -S apple-tool:,apple: -s -k "${KEYCHAIN_PASSWORD}" "${TEMP_KEYCHAIN}" >/dev/null
  security list-keychains -d user -s "${TEMP_KEYCHAIN}" $(security list-keychains -d user | sed 's/[ "]//g')

  if [[ -z "${SIGN_IDENTITY}" ]]; then
    SIGN_IDENTITY="$(security find-identity -v -p codesigning "${TEMP_KEYCHAIN}" | awk -F '"' '/Developer ID Application/ {print $2; exit}')"
  fi
fi

if [[ -z "${SIGN_IDENTITY}" ]]; then
  SIGN_IDENTITY="-"
  echo "No Developer ID certificate configured. Applying ad-hoc signatures for preview builds."
else
  echo "Signing with identity: ${SIGN_IDENTITY}"
fi

sign_bundle() {
  local bundle_path="$1"
  local bundle_name
  bundle_name="$(basename "${bundle_path}")"

  if [[ "${SIGN_IDENTITY}" == "-" ]]; then
    codesign --force --deep --sign "-" "${bundle_path}"
  else
    codesign --force --deep --timestamp --options runtime --sign "${SIGN_IDENTITY}" "${bundle_path}"
  fi

  codesign --verify --deep --strict --verbose=2 "${bundle_path}"
  echo "Signed ${bundle_name}"
}

sign_bundle "${AU_PATH}"
sign_bundle "${VST3_PATH}"
sign_bundle "${APP_PATH}"

echo "macOS code signing step complete."
