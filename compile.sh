#!/usr/bin/env bash
set -e  # Exit on any error

echo "Compiling COL333 Assignment 3 - Metro Line Routing SAT Solver"

CXX="g++"
CXXFLAGS="-Wall -Wextra -O3 -std=c++17"
SRC_DIR="src"

echo "Building encoder..."
if ${CXX} ${CXXFLAGS} -o encoder ${SRC_DIR}/finalenc.cpp; then
    echo "encoder compiled successfully"
else
    echo "Failed to compile encoder"
    exit 1
fi

echo "Building decoder..."
if ${CXX} ${CXXFLAGS} -o decoder ${SRC_DIR}/finaldec.cpp; then
    echo "decoder compiled successfully"
else
    echo "Failed to compile decoder"
    exit 1
fi

# echo "All binaries compiled successfully!"
# echo "Executables created:"
# echo "  - encoder (SAT formula generator)"
# echo "  - decoder (Solution parser)"

chmod +x encoder decoder

# echo "Usage:"
# echo "  ./encoder <input.metromap> <output.cnf>"
# echo "  ./decoder <solution.sat> <output.city>"