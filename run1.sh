#!/usr/bin/env bash
set -euo pipefail

if [ $# -ne 1 ]; then
  echo "Usage: $0 <basename>"
  echo "Example: $0 test   # expects test.city, writes test.satinput"
  exit 1
fi

BASE="$1"
CITY="${BASE}.city"
SATIN="${BASE}.satinput"

if [ ! -f "$CITY" ]; then
  echo "Error: missing input $CITY"
  exit 1
fi

# encoder: <input_file> <output_cnf_file>
./encoder "$CITY" "$SATIN"

# Your encoder also writes the mapping as <cnf>.map (i.e., ${BASE}.satinput.map)
MAP="${SATIN}.map"
if [ ! -f "$MAP" ]; then
  echo "Error: expected mapping file $MAP (written by encoder) not found."
  exit 1
fi

echo "Wrote $SATIN and $MAP"
