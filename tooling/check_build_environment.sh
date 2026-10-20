#!/usr/bin/env bash
# Non-destructive host prerequisite check; never contacts a PS5.
set -u
printf 'PS5-Toolchain-Check (HOST, keine PS5-Verbindung)\n'
for cmd in git make clang++ llvm-ar llvm-ranlib ld.lld zip unzip python3; do
  if command -v "$cmd" >/dev/null 2>&1; then
    printf 'OK      %-13s %s\n' "$cmd" "$(command -v "$cmd")"
  else
    printf 'FEHLT   %-13s\n' "$cmd"
  fi
done
if [[ -f "${KIT_DIR:-/no/kit}/Makefile" ]]; then
  printf 'OK      KIT_DIR: %s\n' "$KIT_DIR"
else
  printf 'FEHLT   vollständiges PS5 Homebrew UI Kit (Quellcode/Build-SDK)\n'
fi
if getent hosts github.com >/dev/null 2>&1; then
  echo 'OK      DNS github.com'
else
  echo 'BLOCKER DNS github.com: in dieser Umgebung nicht auflösbar'
fi
