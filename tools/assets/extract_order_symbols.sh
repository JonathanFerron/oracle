#!/usr/bin/env bash
# extract_order_symbols.sh
# Extracts the 5 Order glyphs (Jonathan's "bubble" line-art versions of his
# printed cards' ASCII order markers: o/+/-/x/| for Orders A-E) from the
# combined source sheet into standalone SVG + PNG files under
# assets/orders/, one per Order, matching game_types.h's ChampionOrder
# enum order (ORDER_A..ORDER_E, i.e. "Dawn Light".."Moonlight" --
# see game_types.h's own comment for the species groupings).
#
# Re-run this after editing the source SVG in Inkscape (e.g. Jonathan
# tweaking stroke widths) rather than re-extracting by hand.
#
# Usage: tools/assets/extract_order_symbols.sh ["path/to/Order Symbols.svg"]
# Run from the repo root. Requires Inkscape (tested against 1.4.3).

set -euo pipefail

SRC="${1:-/mnt/nougatoctet/oracle/Order Symbols.svg}"
OUT_DIR="assets/orders"
PNG_HEIGHT=512

if [ ! -f "$SRC" ]; then
  echo "error: source SVG not found: $SRC" >&2
  exit 1
fi

mkdir -p "$OUT_DIR"

# order letter -> (element id in the source SVG, ascii symbol)
# Confirmed 2026-09-23 by rasterizing the full sheet and inspecting it
# visually, plus querying each id's bounding-box X position (left to right
# matches document order): circle=o, g18754=+, path19178=- (horizontal
# ellipse), g13535=x, ellipse19687=| (ellipse rotated 90 degrees).
declare -A ORDER_IDS=(
  [a]="circle5286:o"
  [b]="g18754:+"
  [c]="path19178:-"
  [d]="g13535:x"
  [e]="ellipse19687:|"
)

for letter in a b c d e; do
  entry="${ORDER_IDS[$letter]}"
  id="${entry%%:*}"
  symbol="${entry##*:}"
  svg_out="$OUT_DIR/order_${letter}.svg"
  png_out="$OUT_DIR/order_${letter}.png"

  inkscape --export-type=svg --export-plain-svg \
    --export-id="$id" --export-id-only \
    --export-filename="$svg_out" "$SRC"

  inkscape --export-type=png --export-height="$PNG_HEIGHT" \
    --export-id="$id" --export-id-only \
    --export-filename="$png_out" "$SRC"

  echo "order_${letter} (\"$symbol\"): $id -> $svg_out, $png_out"
done
