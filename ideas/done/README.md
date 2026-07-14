# Implemented ideas (archive)

Design notes moved here once their feature has been implemented in `src/` and
verified (regression check + valgrind). Kept for historical rationale/intent;
the code in `src/` and the design docs (`doc/oracle_design.md`,
`doc/oracle_todo.md`) are authoritative for current behavior, not these notes.

- `2 Recall Card functionality in cli mode/` — recall mechanic (`card_actions.c`, `cli_input.c`).
- `3 champion combat results display in cli mode/` — combat breakdown display (`combat.c`'s `resolve_combat_with_details`, `cli_display.c`'s `display_combat_details_cli`).
- `4 Display Discard Pile in CLI mode/` — discard pile display (`cli_display.c`'s `display_player_discard`/`_detailed`).
- `5 cash card functionality in cli mode/` — interactive cash exchange (`card_actions.c`'s `play_cash_card_interactive`).
