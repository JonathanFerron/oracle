#!/usr/bin/env bash
# extract_species_colour_icons.sh
# Extracts the 15 coloured species icons embedded in the card-back print
# sheet ("dos des cartes.svg" -- a 3x3 grid of identical card backs, each
# showing all 15 species icons in miniature) into standalone SVG + PNG
# files under assets/species/, as <species>_colour.svg/.png alongside the
# black outline versions from import_species_emblems.sh.
#
# The sheet has no id/text labels naming each icon, so identification
# relies on each coloured icon sharing its exact SVG bounding-box
# dimensions with its black-outline counterpart in assets/species/*.svg
# (same source art, recoloured and placed into the card-back layout) --
# confirmed 2026-09-23 by matching all 15 sizes uniquely and cross-checking
# the result against a screenshot Jonathan provided of the rendered sheet
# (every icon matched exactly). Only one of the 9 identical card-back
# copies is extracted from (the top-left one, x<250mm and y<350mm in
# document units).
#
# Usage: tools/assets/extract_species_colour_icons.sh ["path/to/dos des cartes.svg"]
# Run from the repo root, AFTER import_species_emblems.sh (the black
# outline set must already exist in assets/species/ for the size match).
# Requires Inkscape (tested against 1.4.3) and python3.

set -euo pipefail

SRC="${1:-../oracle outside git/cartes/dos des cartes.svg}"
OUT_DIR="assets/species"
PNG_HEIGHT=512

if [ ! -f "$SRC" ]; then
  echo "error: source SVG not found: $SRC" >&2
  exit 1
fi

SPECIES_LIST="human elf dwarf orc goblin dragon hobbit centaur minotaur aven cyclops faun fairy koatl lycan"

query_file=$(mktemp)
match_file=$(mktemp)
trap 'rm -f "$query_file" "$match_file"' EXIT

inkscape --query-all "$SRC" > "$query_file" 2>/dev/null

python3 - "$query_file" "$OUT_DIR" $SPECIES_LIST > "$match_file" << 'PYEOF'
import subprocess, sys

query_file, out_dir, *species_list = sys.argv[1:]

targets = {}
for name in species_list:
    w = subprocess.run(["inkscape", f"--query-width", f"{out_dir}/{name}.svg"],
                        capture_output=True, text=True).stdout.strip()
    h = subprocess.run(["inkscape", f"--query-height", f"{out_dir}/{name}.svg"],
                        capture_output=True, text=True).stdout.strip()
    targets[name] = (float(w), float(h))

rows = []
with open(query_file) as f:
    for line in f:
        parts = line.strip().split(",")
        if len(parts) == 5:
            id_, x, y, w, h = parts
            try:
                rows.append((id_, float(x), float(y), float(w), float(h)))
            except ValueError:
                pass

for name, (tw, th) in targets.items():
    matches = [r for r in rows
               if abs(r[3] - tw) < 0.05 and abs(r[4] - th) < 0.05
               and r[1] < 250 and r[2] < 350]
    groups = [m for m in matches if m[0].startswith("g")]
    pick = groups[0] if groups else matches[0]
    print(f"{name}\t{pick[0]}")
PYEOF

while IFS=$'\t' read -r name id; do
  svg_out="$OUT_DIR/${name}_colour.svg"
  png_out="$OUT_DIR/${name}_colour.png"

  inkscape --export-type=svg --export-plain-svg \
    --export-id="$id" --export-id-only \
    --export-filename="$svg_out" "$SRC" 2>&1 | grep -v "^Exporting" || true
  inkscape --export-type=png --export-height="$PNG_HEIGHT" \
    --export-id="$id" --export-id-only \
    --export-filename="$png_out" "$SRC" 2>&1 | grep -v "^Exporting" || true

  echo "${name}_colour: $id -> $svg_out, $png_out"
done < "$match_file"
