#!/usr/bin/env bash
set -euo pipefail

# Detect distro tag for output path
source /etc/os-release
DISTRO_TAG="$(echo "${ID,,}-${VERSION_ID}" | tr -d '"')"

# Read package metadata from debian/changelog
PKG_NAME="$(dpkg-parsechangelog -S Source)"
PKG_VER="$(dpkg-parsechangelog -S Version)"

# Normalise version for filename part
PKG_VER_FILE="${PKG_VER#*:}"

ARCH="$(dpkg-architecture -qDEB_BUILD_ARCH)"

# Reproducibility: pin SOURCE_DATE_EPOCH from changelog
export SOURCE_DATE_EPOCH="$(date -d "$(dpkg-parsechangelog -S Date)" +%s)"

# Output dir
OUTDIR="build-deb/${DISTRO_TAG}/${PKG_VER_FILE}-${ARCH}"
mkdir -p "${OUTDIR}"

# Clean (no root needed if Rules-Requires-Root: no)
if [ -f debian/rules ]; then
  debian/rules clean || true
fi

# Build (binary-only), no signing, parallel, skip tests
export DEB_BUILD_OPTIONS="${DEB_BUILD_OPTIONS:-} nocheck parallel=$(nproc)"
dpkg-buildpackage -b -uc -us -j"$(nproc)"

# Move only our artifacts
# main .deb
shopt -s nullglob
for f in ../${PKG_NAME}_*.deb ../${PKG_NAME}_*.ddeb ../${PKG_NAME}_*.buildinfo ../${PKG_NAME}_*.changes; do
  mv -v "$f" "${OUTDIR}/"
done
shopt -u nullglob

echo "Artifacts in: ${OUTDIR}"

# Cleanup
rm -rf obj-x86_64-linux-gnu
