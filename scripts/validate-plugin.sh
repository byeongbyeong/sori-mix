#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PLUGINVAL="${PLUGINVAL:-/Applications/pluginval.app/Contents/MacOS/pluginval}"
BUILD_CONFIG="${BUILD_CONFIG:-Debug}"
BUILD_DIR="${ROOT_DIR}/build"
ARTEFACT_DIR="${BUILD_DIR}/SoriMix_artefacts/${BUILD_CONFIG}"
LOG_DIR="${ROOT_DIR}/tmp/pluginval"
SEED="${PLUGINVAL_SEED:-23063}"
STRICTNESS="${PLUGINVAL_STRICTNESS:-5}"

VST3_PATH="${ARTEFACT_DIR}/VST3/SoriMix.vst3"
AU_PATH="${ARTEFACT_DIR}/AU/SoriMix.component"
USER_AU_DIR="${HOME}/Library/Audio/Plug-Ins/Components"

if [[ ! -x "${PLUGINVAL}" ]]; then
  echo "pluginval not found at: ${PLUGINVAL}" >&2
  echo "Install it with: brew install --cask pluginval" >&2
  exit 1
fi

cmake --build "${BUILD_DIR}" --config "${BUILD_CONFIG}" --parallel

mkdir -p "${LOG_DIR}"

"${PLUGINVAL}" \
  --validate "${VST3_PATH}" \
  --strictness-level "${STRICTNESS}" \
  --skip-gui-tests \
  --random-seed "${SEED}" \
  --timeout-ms 60000 \
  --output-dir "${LOG_DIR}" \
  --output-filename SoriMix-VST3.log

mkdir -p "${USER_AU_DIR}"
cp -R "${AU_PATH}" "${USER_AU_DIR}/"
auval -v aufx SoMx Sori
