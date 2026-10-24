#!/usr/bin/env bash
#
# Regression test: a JPEG whose first segment after SOI is not an APPn
# marker must still be recognized as a JPEG by cfImageOpen().
#
# cfImageOpenFP() identified JPEG input by the three Start-of-Image
# bytes followed by an APPn marker (0xE0-0xEF: JFIF, Exif, ICC, ...).
# JPEG/JFIF do not require an APPn segment, and web-optimized images
# often have them stripped, so such a file starts with SOI followed
# directly by a DQT (0xDB), DHT (0xC4) or SOFn (0xC0-0xCF) segment.
# Every other JPEG decoder opens these, but cfImageOpen() returned NULL
# and the image filters failed with "Unable to open image file for
# printing!".
#
# Like testfilters.sh, this runs from the top-level build directory.
#
set -euo pipefail

TESTIMAGE="./testimage"
if [[ ! -x "${TESTIMAGE}" ]]; then
  echo "testimage program not found at ${TESTIMAGE}" >&2
  exit 99
fi

FIXTURE="cupsfilters/test_files/jpeg-no-app-marker.jpg"
if [[ ! -f "${FIXTURE}" ]]; then
  echo "test file not found: ${FIXTURE}" >&2
  exit 99
fi

# The fixture is a 16x16 grayscale baseline JPEG with every APPn
# segment removed: its header is FF D8 FF DB (SOI, DQT).
header="$(od -An -tx1 -N4 "${FIXTURE}" | tr -d ' \n')"
if [[ "${header}" != "ffd8ffdb" ]]; then
  echo "unexpected fixture header: ${header}" >&2
  exit 99
fi

OUT="$(mktemp --suffix=.pgm)"
trap 'rm -f "${OUT}"' EXIT

if ! "${TESTIMAGE}" "${FIXTURE}" "${OUT}" >/dev/null 2>&1; then
  echo "FAIL: cfImageOpen() rejected a JPEG without an APPn marker" >&2
  exit 1
fi

# testimage writes a binary PGM of the decoded image: "P5\n16\n16\n255\n".
{ read -r magic; read -r width; read -r height; } < "${OUT}"
if [[ "${magic}" != "P5" || "${width}" != "16" || "${height}" != "16" ]]; then
  echo "FAIL: unexpected output: ${magic} ${width}x${height}" >&2
  exit 1
fi

echo "PASS: JPEG without APPn marker opened as a 16x16 image"
