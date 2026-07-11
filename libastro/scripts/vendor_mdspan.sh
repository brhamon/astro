#!/usr/bin/env bash
#
# vendor_mdspan.sh -- fetch the Kokkos reference implementation of std::mdspan
# into third_party/mdspan. This provides std::mdspan today; when the toolchain
# ships a native <mdspan>, delete third_party/mdspan and the CMake logic falls
# back to the standard header (see cmake/mdspan.cmake).
#
set -euo pipefail

# Pin to a branch/tag for reproducibility. `stable` tracks the reference impl's
# release-quality branch; replace with a specific tag/commit to freeze.
REF="${MDSPAN_REF:-stable}"
REPO="https://github.com/kokkos/mdspan.git"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEST="$SCRIPT_DIR/../third_party/mdspan"

if [ -d "$DEST/.git" ]; then
  echo "[vendor_mdspan] updating existing checkout in third_party/mdspan"
  git -C "$DEST" fetch --depth 1 origin "$REF"
  git -C "$DEST" checkout FETCH_HEAD
else
  echo "[vendor_mdspan] cloning $REPO ($REF) into third_party/mdspan"
  rm -rf "$DEST"
  git clone --depth 1 --branch "$REF" "$REPO" "$DEST"
fi

echo "[vendor_mdspan] done. mdspan headers at third_party/mdspan/include"
