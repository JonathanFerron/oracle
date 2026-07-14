# CLI interactive smoke-test scripts

Each `.txt` file here is a canned sequence of stdin lines for a manual,
repeatable run of the interactive CLI (`stda.cli`) mode. They rely on the
fixed PRNG seed (`-p`, i.e. `M_TWISTER_SEED`) for determinism -- the exact
hands/draws only stay stable as long as nothing upstream of these scenarios
(deck order, mulligan logic, turn sequencing) changes. Naming: `hvh_*` scripts
select Human vs Human mode; `hva_*` scripts use the default Human vs AI mode.

## Usage

```bash
./bin/oracle -sl -p < testsrc/cli_scripts/<name>.txt
```

Unlike `bin/expectedresults.txt` (which is diffed byte-for-byte against
`stda.auto` output), these are not auto-asserted -- output includes ANSI
color codes and free-form prompts that aren't worth pinning exactly. Read the
transcript and confirm it matches the description below for whichever script
you ran.

## Scripts

- `hvh_cash_exchange.txt` -- Human vs Human; Player B plays an exchange card
  and picks a specific champion (not the suggested one) to confirm free
  choice + correct hand/luna bookkeeping. Look for "Exchanged DWARF" and the
  hand/luna counts shifting accordingly.
- `hva_discard_display.txt` -- Human vs AI; exercises `gmst` (discard summary)
  and `shod` (detailed discard) commands. Look for both players' discard
  counts and, after `shod`, per-card power-sorted detail.
- `hva_recall.txt` -- Player A attacks and loses a champion to discard, takes
  a hit next turn, then plays a draw/recall card and recalls that exact
  champion back to hand. Look for "Recalled 1 champion(s)" and the champion
  reappearing in the following turn's hand listing.
- `hva_combat_display.txt` -- Forces a combat and shows the per-champion
  roll/combo/damage breakdown ("=== Combat Resolution ===" section) instead
  of the old silent resolution.
