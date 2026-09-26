#!/usr/bin/env bash
# import_species_emblems.sh
# Copies Jonathan's 15 species emblem SVGs (one standalone file per
# species already, unlike the Order/currency sheets) into assets/species/,
# renamed to match game_types.h's ChampionSpecies enum (English, lowercase)
# rather than their French source filenames, and rasterizes each to PNG.
#
# Usage: tools/assets/import_species_emblems.sh ["path/to/emblemes especes"]
# Run from the repo root. Requires Inkscape (tested against 1.4.3).

set -euo pipefail

SRC_DIR="${1:-/mnt/nougatoctet/oracle/emblemes especes}"
OUT_DIR="assets/species"
PNG_HEIGHT=512

if [ ! -d "$SRC_DIR" ]; then
  echo "error: source directory not found: $SRC_DIR" >&2
  exit 1
fi

mkdir -p "$OUT_DIR"

# ChampionSpecies (English, lowercase, matches game_types.h's enum order)
# -> French source filename (without .svg).
declare -A SPECIES_FILES=(
  [human]=humain
  [elf]=elfe
  [dwarf]=nain
  [orc]=orque
  [goblin]=gobelin
  [dragon]=dragon
  [hobbit]=hobbit
  [centaur]=centaure
  [minotaur]=minotaure
  [aven]=aven
  [cyclops]=cyclope
  [faun]=faune
  [fairy]="fée"
  [koatl]=koatl
  [lycan]=lycan
)

for species in human elf dwarf orc goblin dragon hobbit centaur minotaur aven cyclops faun fairy koatl lycan; do
  src_name="${SPECIES_FILES[$species]}"
  src="$SRC_DIR/$src_name.svg"
  svg_out="$OUT_DIR/$species.svg"
  png_out="$OUT_DIR/$species.png"

  if [ ! -f "$src" ]; then
    echo "warning: missing source for $species ($src)" >&2
    continue
  fi

  inkscape --export-type=svg --export-plain-svg \
    --export-filename="$svg_out" "$src"
  inkscape --export-type=png --export-height="$PNG_HEIGHT" \
    --export-filename="$png_out" "$src"

  echo "$species: $src_name.svg -> $svg_out, $png_out"
done
