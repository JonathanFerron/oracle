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
