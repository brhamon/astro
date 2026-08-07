#!/usr/bin/env bash
#
# fetch_catalog.sh -- download the Hipparcos catalogue (van Leeuwen 2007 "New
# Reduction") with curl and verify its SHA-256. Same "fetch big data, don't
# commit" policy as fetch_ephemeris.sh; distributed by the CDS (Strasbourg).
#
# Catalogue: I/311 "Hipparcos, the New Reduction" (van Leeuwen 2007) -- a
# reprocessing of the 1989-1993 raw data with markedly better astrometry
# (parallaxes especially). 117955 stars, ICRS positions at epoch J1991.25.
#
# Usage:
#   scripts/fetch_catalog.sh
#
# Result:
#   data/hip2.dat     the decompressed catalogue (what StarCatalog opens)
set -euo pipefail

URL="https://cdsarc.cds.unistra.fr/ftp/I/311/hip2.dat.gz"
CKSUM="c45d6325bd59dd691764af173a9702e543804a2b6c1d9fea59210e8332e50a4a"  # hip2.dat (decompressed)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DATA_DIR="$SCRIPT_DIR/../data"
DEST="$DATA_DIR/hip2.dat"
GZ="$DATA_DIR/hip2.dat.gz"

mkdir -p "$DATA_DIR"

if [ -f "$DEST" ] && echo "$CKSUM  $DEST" | sha256sum --check --status; then
  echo "[fetch_catalog] hip2.dat already present and verified"
else
  echo "[fetch_catalog] downloading $URL"
  curl --fail --location --connect-timeout 20 -o "$GZ" "$URL"
  gunzip -f "$GZ"  # -> hip2.dat, removes the .gz
  if ! echo "$CKSUM  $DEST" | sha256sum --check --status; then
    echo "error: checksum mismatch for $DEST" >&2
    rm -f "$DEST"
    exit 1
  fi
  echo "[fetch_catalog] verified hip2.dat (117955 stars, 2007 re-reduction)"
fi
