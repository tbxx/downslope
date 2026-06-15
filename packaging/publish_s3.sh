#!/usr/bin/env bash
# Uploads the built Downslope installers to a public-read S3 bucket and prints
# shareable download URLs. Version comes from CMakeLists.txt (single source).
#
# Prereqs:
#   - AWS CLI installed + configured: brew install awscli && aws configure
#   - A public-read bucket created (one-time; see packaging/README.md)
#   - Installers already built/downloaded into the repo:
#       packaging/macos/Downslope-<ver>-macOS.pkg     (built locally)
#       packaging/windows/Downslope-<ver>-Windows.exe (downloaded from CI artifact)
#
# Usage:
#   packaging/publish_s3.sh                      # defaults to bucket x09music-downloads
#   S3_BUCKET=other-bucket packaging/publish_s3.sh
#   S3_REGION=us-east-1 packaging/publish_s3.sh  # region defaults to the CLI's config

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

S3_BUCKET="${S3_BUCKET:-x09music-downloads}"
REGION="${S3_REGION:-$(aws configure get region || true)}"
REGION="${REGION:-us-east-1}"

VERSION="$(sed -n 's/.*project(Downslope VERSION \([0-9.]*\).*/\1/p' "${ROOT}/CMakeLists.txt")"
VERSION="${VERSION:-0.0.0}"

MAC_PKG="${ROOT}/packaging/macos/Downslope-${VERSION}-macOS.pkg"
WIN_EXE="${ROOT}/packaging/windows/Downslope-${VERSION}-Windows.exe"

uploaded=0
upload() {
  local file="$1"
  if [[ ! -f "$file" ]]; then
    echo "  skip (not found): ${file#$ROOT/}" >&2
    return
  fi
  local key; key="$(basename "$file")"
  # octet-stream so browsers download rather than try to render.
  aws s3 cp "$file" "s3://${S3_BUCKET}/${key}" --content-type application/octet-stream >/dev/null
  echo "  https://${S3_BUCKET}.s3.${REGION}.amazonaws.com/${key}"
  uploaded=$((uploaded + 1))
}

echo "Publishing Downslope ${VERSION} to s3://${S3_BUCKET} ..."
upload "$MAC_PKG"
upload "$WIN_EXE"

if [[ "$uploaded" -eq 0 ]]; then
  echo "error: nothing uploaded — build the installers first." >&2
  exit 1
fi
