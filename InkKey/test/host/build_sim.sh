#!/usr/bin/env bash
# Builds the host simulator: real firmware sources + real FreeInkUI against the
# stub hardware headers in stubs/.
set -euo pipefail
cd "$(dirname "$0")"

mkdir -p out

g++ -std=c++17 -g -O1 \
  -Istubs \
  -I../../src \
  -I../../freeink-sdk/libs/ui/FreeInkUI/include \
  -o sim \
  simulate.cpp \
  ../../src/main.cpp \
  ../../src/core/Sha.cpp \
  ../../src/core/Totp.cpp \
  ../../src/core/Vault.cpp \
  ../../src/core/Clock.cpp \
  ../../src/core/Console.cpp \
  ../../freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp

echo "built ./sim"
