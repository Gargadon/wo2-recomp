#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
cmake --preset linux-amd64-release -DREXSDK_DIR="$(dirname "$PWD")" -DCMAKE_CXX_FLAGS=-mssse3
cmake --build --preset linux-amd64-release --target warriors_orochi_2_codegen -j 6
cmake --preset linux-amd64-release
cmake --build --preset linux-amd64-release -j 6
