#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
g++ -std=c++17 -Wall -Wextra -Werror -I"$repo_root/native_ps5" "$repo_root/tests/test_bgh_addons.cpp" -o "$work/bgh-tests"
"$work/bgh-tests"
echo "BGH gallery/storage/layout helper tests: OK"
