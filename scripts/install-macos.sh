#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
AU_SRC="$ROOT/build/EasyLooper_artefacts/Release/AU/Easy Looper.component"
VST3_SRC="$ROOT/build/EasyLooper_artefacts/Release/VST3/Easy Looper.vst3"

AU_DST="$HOME/Library/Audio/Plug-Ins/Components"
VST3_DST="$HOME/Library/Audio/Plug-Ins/VST3"

if [[ ! -d "$AU_SRC" || ! -d "$VST3_SRC" ]]; then
  echo "Brak zbudowanego pluginu. Najpierw:"
  echo "  cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build"
  exit 1
fi

mkdir -p "$AU_DST" "$VST3_DST"
rm -rf "$AU_DST/Easy Looper.component" "$VST3_DST/Easy Looper.vst3"
cp -R "$AU_SRC" "$AU_DST/"
cp -R "$VST3_SRC" "$VST3_DST/"

echo "Zainstalowano:"
echo "  $AU_DST/Easy Looper.component"
echo "  $VST3_DST/Easy Looper.vst3"
echo "Teraz w FL Studio: Options → Manage plugins → Find plugins"
