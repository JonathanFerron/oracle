#!/usr/bin/env bash
set -euo pipefail

SRC="/home/jonathan/projects/oracle/assets/champions/ave6_Aven_Aero_hand_coloured.pdf"
OUT="/home/jonathan/projects/oracle/assets/champions/ave6_Aven_Aero_hand_coloured.png"
TMP="/tmp/ave6_render.png"

pdftoppm -r 600 -png -singlefile "$SRC" "${TMP%.png}"

convert "$TMP" -rotate 90 -fuzz 2% -trim +repage -bordercolor white -border 30x30 "$TMP"

W=$(identify -format "%w" "$TMP")
H=$(identify -format "%h" "$TMP")
TARGET_H=$(( W * 71 / 47 ))
if [ "$TARGET_H" -gt "$H" ]; then
  convert "$TMP" -gravity center -background white -extent ${W}x${TARGET_H} "$OUT"
else
  TARGET_W=$(( H * 47 / 71 ))
  convert "$TMP" -gravity center -background white -extent ${TARGET_W}x${H} "$OUT"
fi

identify "$OUT"
