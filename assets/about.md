# About this folder

Top-level home for shipped binary/data assets the game loads at runtime -- sibling to
`src/`/`bin/`/`doc/`, not inside `src/` (which is exclusively human-authored `.c`/`.h`
source in this project; a binary blob there would break that invariant for no
benefit).

**Category-scoped per subfolder** (`assets/<category>/...`), deliberately not a flat
dump -- first established 2026-09-02 for `A11`'s (AlphaOracle Prime) trained value-net
weights (`assets/ismctsnn/`), with the future SDL3 GUI's champion artwork
(`assets/champions/` or similar, see `doc/oracle_roadmap.md`'s "Next Up") expected to
be the next subfolder. Each subfolder should carry its own README or sidecar file(s)
documenting what the assets are and how they were produced -- see
`assets/ismctsnn/prime_657k_weights.json` for the pattern (architecture, training
provenance, corpus composition, measured results).

## Current contents

- `ismctsnn/` -- `A11` AlphaOracle Prime's trained value-net weights
  (`prime_657k_weights.bin`, loaded by `ismctsnn_load_weights()`) and its
  `.json` provenance sidecar.
- `puct/` -- `A14` AlphaOracle Prime Plus I's trained two-head (value +
  policy) net weights (`plus1_weights.bin`, loaded by `puct_load_weights()`)
  and its `.json` provenance sidecar, following `ismctsnn/`'s own shape.
- SDL3 GUI art (added 2026-09-23, see `doc/oracle_roadmap.md`'s "SDL3 GUI"
  item and `~/.claude/plans/let-s-please-make-a-noble-neumann.md`), sourced
  from Jonathan's own art under `../oracle outside git/` (sibling to this
  repo, not checked in) via `tools/assets/` import scripts -- each
  subfolder is a manifest of *committed* selections, never the full source
  pool (most of which is uncleared web reference material):
  - `champions/` -- champion portraits, one per `fullDeck[]` champion,
    556x839, imported from Jonathan's `<prefix><n>_<Species>_<Name>.jpg`
    picks (`tools/assets/import_champion_art.sh`).
  - `fractals/` -- the legacy/fallback card-art toggle: 18 rendered
    fractal designs x 3 colour variants (54 PNGs, already 556x839; a
    further 16 designs exist only as GIMP Fractal Explorer recipes, not
    yet rendered -- see the fractal inventory note in the GUI plan file).
  - `orders/` -- the 5 Order glyphs (Dawn/Verdant/Ember/Eternal/Moonlight
    Light), extracted from the `cartes champions pgN.svg` print-sheet
    source, rasterised to PNG.
  - `species/` -- the 15 species emblem glyphs, rasterised from
    `emblemes especes/*.svg`.
  - `currency/` -- luna/opale currency symbols, rasterised from
    `symboles monnaie.svg`.
  - `backgrounds/` -- table background tiles.
  - `logo/` -- `oracle_logo.png` (from `logo base/choix final.png`,
    1024x1024).
  - `fonts/` -- `ModernRifgoRegular-MAvdP.otf` (the base logo/UI font) and
    any Google Fonts picks added later. Licence terms not yet verified for
    every font here -- check before any public distribution.
