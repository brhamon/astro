#!/usr/bin/env bash
#
# fetch_ephemeris.sh -- download a JPL binary planetary ephemeris with curl and
# verify its SHA-256. This is the one piece of the legacy top-level Makefile's
# machinery that libastro retains; NOVAS download/patch/build is intentionally
# NOT reproduced here.
#
# Usage:
#   scripts/fetch_ephemeris.sh [DE]        # DE in {440, 430, 431}; default 440
#
# Result:
#   data/<basename>       the downloaded ephemeris
#   data/JPLEPH           symlink pointing at it (what the library opens by default)
#
set -euo pipefail

DE="${1:-440}"

# JPL migrated off FTP to HTTPS; these mirror the legacy Makefile's targets.
# The SHA-256 values are content hashes and therefore stable across the move.
case "$DE" in
  440)
    URL="https://ssd.jpl.nasa.gov/ftp/eph/planets/Linux/de440/linux_p1550p2650.440"
    CKSUM="29915576d0a6555766b99485ac3056ee415e86df4fce282611c31afb329ad062"
    ;;
  430)
    URL="https://ssd.jpl.nasa.gov/ftp/eph/planets/Linux/de430/linux_p1550p2650.430"
    CKSUM="0deb23ca9269496fcbab9f84bec9f3f090e263bfb99c62428b212790721de126"
    ;;
  431)
    URL="https://ssd.jpl.nasa.gov/ftp/eph/planets/Linux/de431/lnxm13000p17000.431"
    CKSUM="fe3d0323d26ada11f8d8228fda9ca590c7eb00cee8b22dff1839f74f5be71149"
    ;;
  *)
    echo "error: unsupported DE '$DE' (expected 440, 430, or 431)" >&2
    exit 2
    ;;
esac

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DATA_DIR="$SCRIPT_DIR/../data"
FILE="$(basename "$URL")"
DEST="$DATA_DIR/$FILE"

mkdir -p "$DATA_DIR"

if [ -f "$DEST" ] && echo "$CKSUM  $DEST" | sha256sum --check --status; then
  echo "[fetch_ephemeris] $FILE already present and verified"
else
  echo "[fetch_ephemeris] downloading $URL"
  curl --fail --location --connect-timeout 20 --no-keepalive -o "$DEST" "$URL"
  if ! echo "$CKSUM  $DEST" | sha256sum --check --status; then
    echo "error: checksum mismatch for $DEST" >&2
    rm -f "$DEST"
    exit 1
  fi
  echo "[fetch_ephemeris] verified $FILE"
fi

ln -sf "$FILE" "$DATA_DIR/JPLEPH"
echo "[fetch_ephemeris] data/JPLEPH -> $FILE"
