#!/usr/bin/env bash
# Build ps5-opengl from the exact public SDK 1.0.2 source commit when the
# upstream binary release is unavailable. No BGH code, titles or PS5 files change.
set -euo pipefail

KIT=${1:?usage: build_pinned_opengl_sdk.sh UI_KIT_DIR SDK_PREFIX [OPENGL_SOURCE_DIR]}
PREFIX=${2:?missing SDK output directory}
SOURCE=${3:-$(dirname "$PREFIX")/ps5-opengl-source}
OPENGL_COMMIT=a519561626933257213d64a4a8bd5e02b55dceb2
OPENGL_REPOSITORY=${BGH_OPENGL_REPOSITORY:-https://github.com/mihawk-99/ps5-opengl.git}

# The UI kit and ps5-opengl must use the same public PS5 Payload SDK.
[[ -s "$KIT/tools/setup-native-dependencies.sh" ]] || {
  echo 'BLOCKER: pinned UI-kit checkout missing setup-native-dependencies.sh' >&2
  exit 21
}

check_prefix() {
  local p=$1 f
  [[ -s "$p/manifest.sha256" ]] || return 1
  for f in include/EGL/egl.h include/GL/glcorearb.h \
    include/ps5_opengl_display.h lib/libPS5OpenGL.a \
    lib/libSceAgc.so lib/libSceAgcDriver.so; do
    [[ -s "$p/$f" ]] || return 1
  done
  (cd "$p" && sha256sum --check --strict --quiet manifest.sha256)
}

if check_prefix "$PREFIX"; then
  echo 'PS5 OpenGL: cached source-built SDK verified using its file manifest.'
  exit 0
fi

# Never silently repair or overwrite a seemingly installed but corrupt SDK.
if [[ -e "$PREFIX" ]]; then
  echo 'BLOCKER: cached PS5 OpenGL SDK exists but failed validation; refusing to overwrite it.' >&2
  exit 22
fi

for cmd in git make python3 clang-18 clang-21 llvm-config-21 ninja meson; do
  command -v "$cmd" >/dev/null || { echo "BLOCKER: OpenGL SDK build tool missing: $cmd" >&2; exit 23; }
done

mkdir -p "$(dirname "$PREFIX")" "$(dirname "$SOURCE")"
if [[ ! -d "$SOURCE/.git" ]]; then
  if [[ -e "$SOURCE" && -n $(ls -A "$SOURCE" 2>/dev/null) ]]; then
    echo 'BLOCKER: SDK source directory exists but is not a Git checkout.' >&2
    exit 24
  fi
  mkdir -p "$SOURCE"
  git -C "$SOURCE" init -q
  git -C "$SOURCE" remote add origin "$OPENGL_REPOSITORY"
fi

if ! git -C "$SOURCE" cat-file -e "$OPENGL_COMMIT^{commit}" 2>/dev/null; then
  GIT_TERMINAL_PROMPT=0 GIT_ASKPASS=/bin/false git -C "$SOURCE" -c credential.helper= \
    fetch --no-tags --depth=1 origin "$OPENGL_COMMIT" || {
      echo 'BLOCKER: archived OpenGL 1.0.2 source commit unavailable.' >&2
      exit 25
    }
fi
git -C "$SOURCE" checkout --detach -q "$OPENGL_COMMIT"
[[ $(git -C "$SOURCE" rev-parse HEAD) == "$OPENGL_COMMIT" ]] || {
  echo 'BLOCKER: SDK source commit does not match the reviewed pin.' >&2
  exit 26
}

# setup-native-dependencies.sh verifies/downloads v0.42 of the public payload SDK.
bash "$KIT/tools/setup-native-dependencies.sh"
PAYLOAD_SDK="$KIT/.deps/native/ps5-payload-sdk"
[[ -x "$PAYLOAD_SDK/bin/prospero-clang" ]] || {
  echo 'BLOCKER: PS5 Payload SDK was not installed.' >&2
  exit 27
}

# Source-fetch verifies immutable commits, Mesa tarball SHA-256, and the PSBC patch.
# Do not substitute a different OpenGL binary or omit the source checks.
export PS5_NATIVE_APP_TEMPLATE="$KIT"
export PS5_PAYLOAD_SDK="$PAYLOAD_SDK"
export LLVM_CONFIG=/usr/bin/llvm-config-21
export PSBC_JOBS="${PSBC_JOBS:-4}"
make -C "$SOURCE" source-fetch
make -C "$SOURCE" sdk-gl46 PS5_OPENGL_SDK_PREFIX="$PREFIX"

check_prefix "$PREFIX" || {
  echo 'BLOCKER: source-built OpenGL SDK did not pass the installed-file manifest check.' >&2
  exit 28
}
printf 'PS5 OpenGL SDK built and verified from source commit %s\n' "$OPENGL_COMMIT"
printf 'PS5_OPENGL_PREFIX=%s\n' "$PREFIX"
