#!/usr/bin/env bash
set -Eeuo pipefail

# Usage: ./compile.sh sample6  (or ./compile.sh input/sample6)
BASE="${1:?Usage: $0 <base-name or path>}"
[[ "$BASE" == */* ]] && TARGET="$BASE" || TARGET="input/$BASE"

echo "[1/6] Cleaning old binaries…"
rm -f encoder decoder

echo "[2/6] Building encoder…"
g++ -std=c++11 -O2 -I src -o encoder src/encoder.cpp
# g++ -std=c++11 -O2 -I src -o encoder src/newenc.cpp

echo "[3/6] Building decoder…"
g++ -std=c++11 -O2 -I src -o decoder src/decoder.cpp
# g++ -std=c++11 -O2 -I src -o decoder src/newdec.cpp

echo "[4/6] Running run1 on ${TARGET}…"
./run1.sh "$TARGET"

echo "[5/6] Running minisat on ${TARGET}.satinput → ${TARGET}.satoutput…"
set +e
minisat "${TARGET}.satinput" "${TARGET}.satoutput"
MS=$?
set -e

case "$MS" in
  10) echo "minisat: SAT"; SAT_STATUS="SAT";;
  20) echo "minisat: UNSAT"; SAT_STATUS="UNSAT";;
   0) echo "minisat: returned 0 (interrupted/unknown)"; SAT_STATUS="UNKNOWN";;
  *)  echo "minisat: error (exit $MS)"; exit "$MS";;
esac

if [[ "$SAT_STATUS" == "SAT" ]] || grep -q "SATISFIABLE" "${TARGET}.satoutput" 2>/dev/null; then
  echo "[6/6] Running run2 on ${TARGET}…"
  ./run2.sh "$TARGET"
  echo "[✓] Running format checker…"
  python3 format_checker.py "$TARGET" --verbose
else
  echo "[i] Instance is UNSAT (or not SAT). Skipping run2/format checker."
fi

echo "Done."
