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
  from Jonathan's own art under `/mnt/nougatoctet/oracle/` (NAS storage,
  moved there 2026-09-26, not checked in) via `tools/assets/` import scripts -- each
  subfolder is a manifest of *committed* selections, never the full source
  pool (most of which is uncleared web reference material):
  - `champions/` -- champion portraits, one per `fullDeck[]` champion,
    556x839, imported from Jonathan's `<prefix><n>_<Species>_<Name>.jpg`
    picks (`tools/assets/import_champion_art.sh`). This same 556x839 target
    (47x71mm @ 300dpi, `../oracle outside git/dimensions image champion.txt`)
    serves both the printed-card SVG and the GUI's rendered card art, so one
    export pipeline covers both consumers.
    **Preferred resize/pad approach** (confirmed available 2026-09-28:
    ImageMagick 7.1.2 `magick`, also Python3/Pillow 12.1.1 and GIMP as
    fallbacks): most source picks are already close to the target aspect
    (e.g. 832x1248, aspect 0.667 vs the target's 0.663) and just need a
    downscale; a few (scans, hand-coloured pieces, e.g.
    `cen5_Centaure_Kentaur.png` at 1718x2483, aspect 0.692) are wider and
    need genuine white padding on two sides rather than just a resize. One
    command handles both cases without distortion or cropping:
    ```
    magick input.jpg -resize 556x839 -background white -gravity center -extent 556x839 output.png
    ```
    `-resize` fits the image within the box preserving aspect ratio;
    `-extent` pads out to the exact target, centered, filling with
    `-background`. Tested against `ave1_Aven_Sourlio.jpg` (near-zero
    padding needed) and `cen5_Centaure_Kentaur.png` (visible left/right
    bars) -- both produced clean 556x839 output.
    **Scans/hand-coloured art need a sampled background, not hardcoded
    white**: their paper background is typically off-white rather than
    pure `#FFFFFF`, so a hardcoded white pad creates a visible seam where
    padding meets the source's own background. Sample a corner pixel of
    the source (e.g. `magick input.jpg -format "%[pixel:p{5,5}]" info:`)
    and pass that as `-background` instead of a literal `white` for that
    subset, rather than assuming pure white project-wide.
  - `fractals/` -- the legacy/fallback card-art toggle, for a "fractales
    champion art" GUI mode (art-style toggle, alternative to the champion
    portraits above). **All 34 designs x 3 colour variants (rouge/horizon
    2/yellow orange) rendered and imported, 102 PNGs, all 556x839, ready
    to wire into the GUI's "fractales champion art" mode.** Copied
    2026-09-28 from the NAS source (`fractale N <variant>.png`) with
    filenames sanitized/zero-padded for asset use:
    `fractale_<NN>_<variant>.png`, `NN` = `01`-`34`, `<variant>` =
    `rouge`/`horizon_2`/`yellow_orange` -- no `import_fractal_art.sh`
    script yet (this pass was a plain `cp`, not scripted), so a future
    re-import from an updated source still needs one, same pattern as
    `import_champion_art.sh` above. Originally 18 of 34 designs were
    rendered by hand (54 PNGs); the remaining 16 (designs 17-25, 28-33;
    design 16 was finished by hand separately) existed only as GIMP
    Fractal Explorer recipes (`fractale N`, a plain-text shape/colour
    spec -- fractal type, viewport bounds, iteration count, cx/cy) until
    **2026-09-28**, when they were batch-rendered headlessly via GIMP
    3.2.2's Python-Fu (`gimp -i --batch-interpreter=python-fu-eval`,
    `plug-in-fractalexplorer`), each recipe's 3 named colour variants
    reproduced as: `rouge` = the plugin's internal `colormap` mode
    (red-invert on, matching the one colour-variant recipe that happened
    to be saved, `fractale 1 rouge`); `horizon 2`/`yellow orange` = the
    plugin's `gradient` mode with GIMP's built-in gradients of those
    exact names set active (confirmed real stock gradient names via
    `gimp-gradients-get-list`) -- verified by re-rendering an
    already-shipped design (`fractale 2`) from its recipe alone and
    diffing visually against the existing PNG (pixel-for-pixel match).
    Matching `.xcf` project files were saved alongside all 15 newly
    rendered designs' PNGs, and design 16's PNGs were exported from
    Jonathan's own hand-made XCFs (already exactly 556x839, no rework
    needed) -- no pre-existing `.png`/`.xcf` was overwritten by this pass.
    GIMP 3's PDB signature for `plug-in-fractalexplorer` differs
    substantially from the legacy 1997 recipe file format (drawables now
    an array, `fractal-type`/`color-mode`/`red-mode`/etc. now string
    nicks instead of integer codes, colour-stretch values now 0.0-1.0
    instead of 0-255, no width/height args) -- worth knowing before
    writing the eventual import script, since a naive port of the old
    settings file won't work as-is.
  - `orders/` -- **done 2026-09-23**: the 5 Order glyphs (`order_a.svg`/
    `.png` .. `order_e.svg`/`.png`, matching `game_types.h`'s `ChampionOrder`
    enum order/index), Jonathan's "bubble" line-art versions of his printed
    cards' ASCII order markers, extracted via
    `tools/assets/extract_order_symbols.sh` from his combined
    `Order Symbols.svg` source sheet (5 shapes left to right). ASCII <->
    shape <-> Order mapping (confirmed by rendering and visual inspection,
    matches `ChampionOrder`'s own species groupings):
    | Order | ASCII | Shape | Light name |
    |---|---|---|---|
    | `ORDER_A` | `o` | circle | Dawn Light |
    | `ORDER_B` | `+` | 4-petal bubble | Verdant Light |
    | `ORDER_C` | `-` | horizontal ellipse | Ember Light |
    | `ORDER_D` | `x` | 4-petal bubble, rotated | Eternal Light |
    | `ORDER_E` | `\|` | vertical ellipse | Moonlight |
  - `species/` -- **done 2026-09-23**: all 15 species emblems, imported via
    the new `tools/assets/import_species_emblems.sh` (each was already its
    own standalone SVG in `emblemes especes/`, unlike the Order/currency
    combined sheets, so this is a straight plain-SVG copy + rasterize, no
    `--export-id` extraction needed). Filenames are English, matching
    `game_types.h`'s `ChampionSpecies` enum, not the French source names:
    `human.svg`/`.png` (from `humain.svg`), `elf` (`elfe`), `dwarf`
    (`nain`), `orc` (`orque`), `goblin` (`gobelin`), `dragon`, `hobbit`,
    `centaur` (`centaure`), `minotaur` (`minotaure`), `aven`, `cyclops`
    (`cyclope`), `faun` (`faune`), `fairy` (`fée`), `koatl`, `lycan`. All
    still black; colour later, same as Aureus/Opale above.
    **Coloured versions added the same date**: `<species>_colour.svg`/
    `.png`, extracted by `tools/assets/extract_species_colour_icons.sh`
    from `cartes/dos des cartes.svg` (the card-back print sheet -- a 3x3
    grid of 9 identical card backs, each showing all 15 species icons in
    miniature; only one copy is extracted from). That sheet has no id/text
    labels naming each icon, so identification relies on each coloured
    icon sharing its exact SVG bounding-box size with its black-outline
    counterpart above (same source art, recoloured) -- all 15 sizes
    matched uniquely and the result was cross-checked against a screenshot
    Jonathan provided of the rendered sheet (every icon matched). Note
    some of the coloured art differs from its black emblem in subject, not
    just colour, once actually compared side by side (e.g. `faun`'s black
    emblem and its `faun_colour` counterpart are both the same goat-horned
    face -- confirmed identical; but this cross-check is what caught and
    corrected an initial mismatch during identification, worth
    re-verifying by eye if this script is ever re-run against updated
    source art).
  - `currency/` -- **done 2026-09-23**: the 3 currency symbols, extracted
    from `symboles monnaie.svg` the same way as the Order glyphs
    (Inkscape `--export-id`/`--export-id-only`, group ids confirmed by
    each shape's bounding-box X position -- left to right = Luna, Aureus,
    Opale). `luna.svg`/`.png` recoloured to `#216778` (used as the GUI's
    window/taskbar icon), then to a lighter `#acacac` (2026-09-25, Jonathan's
    call); `aureus.svg`/`.png` and `opale.svg`/`.png` still black -- colour
    to be set later, trivial to change at the SVG level.
    Opale's symbol was fixed by Jonathan in Inkscape since the first
    extraction attempt (smoothing + the previously-missing small ellipse,
    now present).
  - `backgrounds/` -- table background tiles.
  - `logo/` -- `oracle_logo.png` (from `logo base/choix final.png`,
    1024x1024).
  - `fonts/` -- `ModernRifgoRegular-MAvdP.otf` (the base logo/UI font;
    licence terms not yet verified -- check before any public distribution).
    **Comic Sans MS is NOT here** despite being one of Jonathan's requested
    runtime font options (it's what the printed cards use) -- Microsoft's
    EULA for the `ttf-mscorefonts-installer` package (already installed on
    this box, `/usr/share/fonts/truetype/msttcorefonts/Comic_Sans_MS.ttf`)
    prohibits redistributing the font file itself, which is exactly why that
    package ships as a downloader rather than the `.ttf` directly. The GUI's
    font-swap feature (M1 step 7) should look it up from that well-known
    system path at runtime with a graceful fallback (e.g. to ModernRifgo)
    when it's absent, not bundle it.
    **Comic Sans lookalikes added 2026-09-25** (Jonathan's call, for easier
    redistribution with fewer host dependencies than the EULA-gated system
    lookup above): `ComicNeue-Regular/Bold/Light/Italic/BoldItalic/
    LightItalic.ttf` (all 6 static weights) and `PatrickHand-Regular.ttf`
    (its only weight), both downloaded from the `google/fonts` GitHub repo's
    `ofl/comicneue/` and `ofl/patrickhand/` directories, SIL Open Font
    License 1.1 (freely bundleable) -- licence text alongside as
    `ComicNeue-OFL.txt`/`PatrickHand-OFL.txt`. Comic Neue is the closer
    match (explicitly designed as a Comic Sans redesign); Patrick Hand is a
    rougher handwriting-style alternative. Jonathan is also installing both
    as system fonts on this box manually. Neither is wired into the GUI's
    font-swap feature yet.
