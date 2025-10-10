#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <basename-without-extension>"
  exit 1
fi

base="$1"

# Compile (adjust flags if needed)
g++ -O2 -std=gnu++17 -o encoder encoder.cpp

# Produce DIMACS and varmap for the given basename:
#   reads:  ${base}.city
#   writes: ${base}.satinput and varmap.txt
./encoder "$base"
