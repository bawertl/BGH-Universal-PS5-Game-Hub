#!/usr/bin/env bash
# Fetch the exact reviewed ps5-homebrew-ui commit. The upstream repository may
# become temporarily inaccessible. A public fork contains the SAME Git object,
# so we can safely recover without changing the SDK/UI-kit revision.
set -euo pipefail
KIT=${1:?usage: fetch_pinned_ui_kit.sh KIT_DIR PINNED_COMMIT}
KIT_COMMIT=${2:?missing pinned commit}
if [[ ! $KIT_COMMIT =~ ^[a-f0-9]{40}$ ]]; then
  echo 'BLOCKER: UI-kit pin must be a full 40-character Git commit hash.' >&2
  exit 6
fi
PRIMARY=${BGH_UI_KIT_PRIMARY:-https://github.com/blackbearreloaded/ps5-homebrew-ui.git}
FALLBACK=${BGH_UI_KIT_FALLBACK:-https://github.com/mihawk-99/PS5_VKHomebrewUI.git}

if [[ ! -d "$KIT/.git" ]]; then
  if [[ -e "$KIT" && -n $(ls -A "$KIT" 2>/dev/null) ]]; then
    echo "BLOCKER: Non-empty kit directory is not a Git checkout: $KIT" >&2
    exit 6
  fi
  mkdir -p "$KIT"
  git -C "$KIT" init -q
fi

if ! git -C "$KIT" cat-file -e "$KIT_COMMIT^{commit}" 2>/dev/null; then
  found=0
  for source in "$PRIMARY" "$FALLBACK"; do
    if [[ $source == "$PRIMARY" ]]; then
      echo 'UI-Kit: original source, exact pinned commit'
    else
      echo 'UI-Kit: original unavailable; verified-history fork, SAME pinned commit'
    fi
    git -C "$KIT" remote remove origin 2>/dev/null || true
    git -C "$KIT" remote add origin "$source"
    # Do not prompt for credentials in unattended GitHub Actions.
    # Do not fetch a floating release or replace our reviewed commit with HEAD.
    if GIT_TERMINAL_PROMPT=0 GIT_ASKPASS=/bin/false git -C "$KIT" -c credential.helper= \
        fetch --no-tags --filter=blob:none --depth=1 origin "$KIT_COMMIT"; then
      found=1
      break
    fi
    # Some remote git-upload-pack configurations do not allow fetching an
    # ancestor by SHA directly. Fetch reachable history, then verify the same SHA.
    if GIT_TERMINAL_PROMPT=0 GIT_ASKPASS=/bin/false git -C "$KIT" -c credential.helper= \
        fetch --no-tags --filter=blob:none --depth=128 origin main && \
        git -C "$KIT" cat-file -e "$KIT_COMMIT^{commit}" 2>/dev/null; then
      found=1
      break
    fi
    echo 'UI-Kit: this source is currently unreachable or lacks the required commit.' >&2
  done
  if [[ $found != 1 ]]; then
    echo 'BLOCKER: Neither source provides the PINNED UI-kit revision; refusing an unverified replacement.' >&2
    exit 6
  fi
fi

git -C "$KIT" checkout -q --detach "$KIT_COMMIT"
actual=$(git -C "$KIT" rev-parse HEAD)
if [[ $actual != "$KIT_COMMIT" || ! -s "$KIT/Makefile" ]]; then
  printf 'BLOCKER: UI-kit checkout mismatch or Makefile missing: %s\n' "$actual" >&2
  exit 6
fi
printf 'UI-kit verified at EXACT commit %s\n' "$actual"
