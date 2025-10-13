#!/usr/bin/env bash
set -euo pipefail

if [ $# -ne 1 ]; then
  echo "Usage: $0 <basename>"
  echo "Example: $0 test   # expects test.city, test.satinput, test.satoutput"
  exit 1
fi

BASE="$1"
CITY="${BASE}.city"
SATIN="${BASE}.satinput"
SATOUT="${BASE}.satoutput"
OUT="${BASE}.metromap"
MAP="${SATIN}.map"

# The TA guarantees these exist (plus anything run1.sh created)
for f in "$CITY" "$SATIN" "$SATOUT" "$MAP"; do
  if [ ! -f "$f" ]; then
    echo "Error: missing $f"
    exit 1
  fi
done

# decoder: <input_file> <sat_output_file> <output_file> <mapping_file>
./decoder "$CITY" "$SATOUT" "$OUT" "$MAP"

echo "Wrote $OUT"
