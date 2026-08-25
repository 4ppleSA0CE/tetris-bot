#!/usr/bin/env bash
set -euo pipefail
shopt -s nullglob

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"

: "${EMSDK_ENV:=$HOME/emsdk/emsdk_env.sh}"
if ! command -v em++ >/dev/null 2>&1; then
  # shellcheck disable=SC1090
  source "$EMSDK_ENV" >/dev/null
fi

WANT="$(cat .emscripten-version)"
GOT="$(em++ --version | head -1 | sed -E 's/.* ([0-9]+\.[0-9]+\.[0-9]+) .*/\1/')"
if [ "$WANT" != "$GOT" ]; then
  echo "emscripten version mismatch: .emscripten-version says $WANT, em++ is $GOT" >&2
  exit 1
fi

CORE_SRCS=(core/*.cpp)
if [ ${#CORE_SRCS[@]} -eq 0 ]; then
  echo "no sources found in core/ - is this the repo root?" >&2
  exit 1
fi

mkdir -p dist

em++ -std=c++20 -Oz -flto -Wall -Wextra \
  -I . -I core -I bindings \
  "${CORE_SRCS[@]}" \
  bindings/bot_instance.cpp \
  bindings/embind.cpp \
  -lembind \
  -sEXPORT_ES6=1 \
  -sEXPORT_NAME=createBotModule \
  -sENVIRONMENT=web,node \
  -sSINGLE_FILE=1 \
  -sALLOW_MEMORY_GROWTH=1 \
  -sINITIAL_MEMORY=32MB \
  -sMAXIMUM_MEMORY=256MB \
  -sSTACK_SIZE=1MB \
  -sEXPORTED_RUNTIME_METHODS=HEAPU8 \
  -sEXPORTED_FUNCTIONS=_malloc,_free,_sbrk \
  -sFILESYSTEM=0 \
  -sASSERTIONS=0 \
  --emit-tsd bot.d.ts \
  -o dist/bot.js

ls -l dist/bot.js dist/bot.d.ts
