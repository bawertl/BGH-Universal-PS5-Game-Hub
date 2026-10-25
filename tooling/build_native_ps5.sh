#!/usr/bin/env bash
# PS5 Universal Game Launcher native build – development host only.
# No FTP, no console connection, no extra payloads, no firmware modifications.
set -euo pipefail
PROJECT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
OUT="${OUT:-$PROJECT/build/ps5}"
KIT="${KIT_DIR:-$OUT/ps5-homebrew-ui}"
TITLE_ID="${TITLE_ID:-PPSA99079}"
# SDK-independent pin of the actual UI kit reviewed in this project.
KIT_COMMIT="${KIT_COMMIT:-bbe19054ee2e7ec069254775273aeb8846c4bd3e}"
if [[ ! $TITLE_ID =~ ^PPSA[0-9]{5}$ ]]; then
  printf 'ERROR: TITLE_ID muss PPSA und fünf Ziffern enthalten.\n' >&2; exit 2
fi
for binary in git make python3 clang++ llvm-ar llvm-ranlib ld.lld unzip zip; do
  if ! command -v "$binary" >/dev/null 2>&1; then
    printf 'BLOCKER: Build-Werkzeug fehlt: %s\n' "$binary" >&2; exit 4
  fi
done
mkdir -p "$OUT"
if [[ ! -f "$KIT/Makefile" ]]; then
  if [[ -n ${KIT_DIR:-} ]]; then
    printf 'BLOCKER: KIT_DIR zeigt auf keinen vollständigen UI-Kit-Checkout: %s\n' "$KIT" >&2; exit 5
  fi
  # Upstream URL may require authentication. Verify SAME immutable commit in public mirror.
  bash "$PROJECT/tooling/fetch_pinned_ui_kit.sh" "$KIT" "$KIT_COMMIT"
fi
if [[ -d "$KIT/.git" ]]; then
  # Also reject a stale/unchecked KIT_DIR supplied by the caller.
  actual=$(git -C "$KIT" rev-parse HEAD)
  if [[ $actual != "$KIT_COMMIT" || ! -s "$KIT/Makefile" ]]; then
    printf 'BLOCKER: Unerwarteter UI-Kit-Stand: %s\n' "$actual" >&2;exit 6
  fi
elif [[ ${ALLOW_UNVERIFIED_KIT:-0} == 1 ]]; then
  printf 'WARNUNG: Nicht verifizierter UI-Kit-Checkout: nur für lokale Simulation, kein Release.\n' >&2
else
  printf 'BLOCKER: UI-Kit ohne Git-Checkout; Commit nicht verifizierbar.\n' >&2
  exit 6
fi
# The pinned UI kit otherwise fetches a ps5-opengl SDK archive from a removed URL.
# GitHub Actions must provide the source-built, verified SDK before running make.
if [[ ${GITHUB_ACTIONS:-} == true && -z ${PS5_OPENGL_PREFIX:-} ]]; then
  echo 'BLOCKER: A verified PS5_OPENGL_PREFIX is required for CI builds.' >&2
  exit 13
fi
if [[ -n ${PS5_OPENGL_PREFIX:-} ]]; then
  [[ -s "$PS5_OPENGL_PREFIX/manifest.sha256" ]] || {
    echo 'BLOCKER: OpenGL SDK manifest missing.' >&2; exit 13;
  }
  (cd "$PS5_OPENGL_PREFIX" && sha256sum --check --strict --quiet manifest.sha256) || {
    echo 'BLOCKER: OpenGL SDK manifest is invalid.' >&2; exit 13;
  }
fi
# stb_image header is needed for JPEG/TGA decode. Pinned immutable GitHub commit.
# The original simulator PNG path stays unchanged. Do not modify game assets.
STB_COMMIT=2c980bb59875b0d32144a71867fbdebb2f77cd20
if [[ ! -s "$KIT/src/concepts/stb_image.h" ]]; then
  curl --fail --location --retry 3 --connect-timeout 20 \
    "https://raw.githubusercontent.com/nothings/stb/${STB_COMMIT}/stb_image.h" \
    -o "$KIT/src/concepts/stb_image.h"
fi
test -s "$KIT/src/concepts/stb_image.h" || { echo 'BLOCKER: stb_image header missing'; exit 11; }
expected_blob=9eedabedc45b3e6fd88fae6f14a160b4d53272ec
actual_blob=$(git hash-object "$KIT/src/concepts/stb_image.h")
if [[ "$actual_blob" != "$expected_blob" ]]; then
  printf 'BLOCKER: stb_image.h Git blob does not match pinned source: %s\n' "$actual_blob" >&2
  exit 12
fi
python3 "$PROJECT/tooling/install_ui_kit_overlay.py" "$KIT"
# BGH V1.1.30: package the selectable images, not just the default assets.
mkdir -p "$KIT/assets/themes"
cp -a "$PROJECT/assets/themes/." "$KIT/assets/themes/"
# The kit sources compile inside src/concepts, not native_ps5.
for addon_header in bgh_theme_gallery.hpp bgh_gallery_ftp.hpp bgh_storage_metrics.hpp bgh_nextgen_layout.hpp; do
  cp "$PROJECT/native_ps5/$addon_header" "$KIT/src/concepts/$addon_header"
done
# The visible build identifier is deliberately ONLY the release number.
# No branch, test name, commit hash, or other decoration is ever displayed.
if grep -Fq '@BGH_GIT_COMMIT@' "$KIT/src/concepts/universal_launcher.cpp"; then
  echo 'BLOCKER: Unremoved legacy commit marker in launcher source' >&2; exit 14
fi
make -C "$KIT" init TITLE_ID="$TITLE_ID" APP_NAME="BGH - BawertL's Game Hub"
# PS5-Home icon comes directly from Stefan's approved BGH logo.
cp "$PROJECT/sce_sys/icon0.png" "$KIT/sce_sys/icon0.png"
# PS5 home-screen selection and launch backgrounds: BC7 UNORM DX10 DDS, 3840x2160.
for background in pic0.dds pic1.dds; do
  test -s "$PROJECT/sce_sys/$background" || {
    echo "BLOCKER: BGH PS5 background missing: sce_sys/$background" >&2; exit 15;
  }
  cp "$PROJECT/sce_sys/$background" "$KIT/sce_sys/$background"
done
# Upstream make fetches the versioned SDK, creates libc.prx, links and packages.
make -C "$KIT" BUILD_JOBS="${BUILD_JOBS:-4}" app
PACKAGE="$KIT/dist/$TITLE_ID"
for file in 'eboot.bin' 'sce_module/libc.prx' 'sce_sys/param.json'; do
  if [[ ! -s "$PACKAGE/$file" ]]; then
    printf 'BLOCKER: Native PS5-Datei fehlt/ist leer: %s\n' "$PACKAGE/$file" >&2;exit 7
  fi
done
if ! grep -Fq '"titleId": "'"$TITLE_ID"'"' "$PACKAGE/sce_sys/param.json"; then
  printf 'BLOCKER: App-Metadaten enthalten falsche Titel-ID.\n' >&2;exit 8
fi
if [[ ! -f "$PACKAGE/assets/paths.conf" ]]; then
  printf 'BLOCKER: Scanpfad-Konfiguration wurde nicht paketiert.\n' >&2;exit 9
fi
# Crucial: DO NOT repack with zip. The upstream PS5 kit fixes every ZIP entry's
# 0777 permissions with zip-open-modes.py (required by ShadowMountPlus/native loader).
# A second generic zip -r silently loses that guarantee.
SOURCE_ZIP="$KIT/dist/$TITLE_ID.zip"
# Empty user image folders are staged under the installed title root.
# They are appended to the upstream ZIP without rebuilding existing entries,
# preserving the native title's signed binaries and PS5 0777 archive modes.
python3 "$PROJECT/tooling/append_user_theme_folders.py" \
  "$SOURCE_ZIP" "$TITLE_ID" "$PROJECT/user_themes"
DEST="$OUT/BGH_BawertLs_Game_Hub_V1_1_30_${TITLE_ID}_PS5_TEST.zip"
python3 "$PROJECT/tooling/verify_ps5_package.py" "$SOURCE_ZIP" "$TITLE_ID"
cp -f -- "$SOURCE_ZIP" "$DEST"
python3 "$PROJECT/tooling/verify_ps5_package.py" "$DEST" "$TITLE_ID"
(cd "$OUT" && sha256sum "${DEST##*/}" > "${DEST##*/}.sha256")
printf '\nNATIVER PS5-BUILD ERFOLGREICH: %s\n' "$DEST"
printf 'PS5-KONSOLENTEST: NOCH NICHT DURCHGEFÜHRT. Keine automatische Installation.\n'
