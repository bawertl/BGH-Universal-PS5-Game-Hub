#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
compiler=${CXX:-clang++}
"$compiler" -std=c++20 -Wall -Wextra -Wpedantic -Werror -fsyntax-only \
  -I"$ROOT/tooling/host_shim" -I"$ROOT/native_ps5" \
  -I"$ROOT" -x c++ "$ROOT/native_ps5/universal_launcher.cpp"
echo 'OK: Native GUI host-shim syntax compilation (not a PS5 build)'
