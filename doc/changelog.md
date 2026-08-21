# Oracle Changelog

Completed work, most recent first. `doc/oracle_todo.md` tracks what's still open;
this file is where finished items go so the todo list doesn't keep growing.

---

## 2026-08-20 — Ideas 2 and 3 cleanup (engine refactoring notes, TUI prototype)

- **Idea 2** (`ideas/2 engine and action system design/`): pared
  down to only mode-agnostic core-engine material. Deleted outright, as superseded
  before ever being built: a full CLI-specific reference implementation
  (`reference_implementation_with_callbacks.c`, `cli_refactor_summary.md`,
  `Refactoring of stda_cli in 4 modules...md`, ~2000 lines) describing an
  engine/`Action*`/`UICallbacks` redesign the real CLI split (2026-07-14) never
  adopted — it took a simpler path (`cli_display`/`cli_action_display`/`cli_input`/
  `cli_io`/`cli_game` around the `UiIO` seam). GUI-flavored sketches
  (`gamegui_main_loop.txt`, `unified_gui_interface.txt`, `stda_game_gui.txt`,
  `stda_game_impl.txt`, plus the GUI example from `mode_usage_examples.txt`) were
  consolidated into new `ideas/9 gui/game_loop_engine_integration_notes.md` — genuinely
  new content, since the existing SDL3 GUI plan there covers rendering/assets/input but
  not engine integration. Network-flavored sketches (`More notes on client-server
  preparation.md`, `client_game_gui.txt`, plus the Server example) were checked against
  `ideas/8 client server/`'s two existing ~5100-line docs — mostly redundant/inferior
  and dropped, but an opaque-handle server/client API sketch, a `CardVisibility` enum,
  and a simpler single-threaded poll-based client loop were confirmed (by grep) *not*
  already present there, so those moved into new
  `ideas/8 client server/game_loop_and_client_api_notes.md`, which also flags that
  ideas/8's own "Strategy Interface" section still shows the stale `void`-mutating
  strategy signature rather than the `Action`-returning one a networked AI client
  actually needs (load-bearing for AI-agent-as-network-client scenarios). Verified
  before any of this that the AI-as-network-client use case itself stays fully covered
  (ideas/8's own "AI Integration" section, untouched). Added a `README.md` to folder 2
  summarizing what remains and why.
- **Idea 3**: nine early flat-file TUI prototype files (`oracle_tui_impl.txt`/
  `_header.txt`, `oracle_cmdline.txt`/`_h.txt`, `oracle_main_updated.txt`,
  `oracle_makefile_tui.txt`, `oracle_version_h.txt`, `oracle_tui_readme.md`,
  `oracle_integration_guide.md` — pre-reorg includes, the old `HDCLL` linked-list
  types, a single `tui.c`, missing most of today's `cmdline.c` options) were confirmed
  superseded by the real `src/ui/tui/` + `src/roles/stda/stda_tui*.c` implementation
  and deleted outright. One file — `ascii art fonts for logo in tui and cli modes.txt`
  — stayed, since it describes a logo feature confirmed (by grep) not implemented
  anywhere. Since nothing TUI-specific remained, the folder was renamed
  `ideas/3 tui/` → `ideas/3 misc ui ideas/`; all repo cross-references to the old path
  updated (`CLAUDE.md`, `doc/oracle_roadmap.md`, `ideas/2 …/target_folder_structure_v4.md`,
  `src/ui/tui/tui display input and callbacks.txt`) except one intentionally-preserved
  historical mention in the already-archived `ideas/done/1 …/pragmatic_cleanup_
  implementation_plan.md`.
- No game-logic or build changes; this is a documentation/`ideas/`-only pass.

## 2026-08-20 — Idea 1 (source folder structure) closed out; doc cleanup pass

- **Idea 1 second pass**: `ideas/done/1 improve source code folder structure/`'s pragmatic
  cleanup (done 2026-07-14) left a small remainder, now closed. The ten `src/`
  placeholder `.txt` files had cross-references to a pre-renumbering `ideas/` layout
  (`ideas/9`, `11`, `12.1`, `15`, `16`, `18`, `1 tui`) — repointed at the current folder
  numbers. `src/ui/cli/cli display input and callbacks.txt`, which its own text marked
  "safe to remove" once superseded by real code, was deleted (confirmed nothing
  references it first). The three test targets (`test_combo`, `test_recall`,
  `test_cash_exchange`) compiled objects straight into `src/**`/`testsrc/` via the
  Makefile's default `%.o: %.c` rule, leaving twelve stray git-ignored `.o` files that
  `make clean` never removed; `makefile` now routes them through `$(BUILDDIR)` (a new
  `$(BUILDDIR)/testsrc/%.o` pattern rule mirrors the existing source rule), and `clean`
  removes the test binaries plus any leftover in-tree `.o` files defensively.
  `make test_stda_auto` invoked `./bin/oracle.exe` (the MSYS2 name) unconditionally, so
  it could never pass on the primary Linux target; fixed to use `$(TARGET)` and marked
  `.PHONY`, with `help` listing all four test targets. The folder itself moved to
  `ideas/done/1 improve source code folder structure/`; its still-relevant target
  architecture doc (`revised_folder_structure.md`) moved instead to
  `ideas/2 engine and action system design/
  target_folder_structure_v4.md`, trimmed of content duplicated elsewhere in that
  folder, since that's where the work it describes actually happens.
- **Doc cleanup**: `doc/oracle_design.md` (dated December 2025, describing a
  pre-reorg flat `src/*.c` tree, missing recall/TUI/fixed-arrays, an ~800-line
  `stda_cli.c` awaiting a split, and Geany/Arch Linux) was rewritten top to bottom
  against the current codebase and restructured around what's actually true today,
  including new UI-architecture (`UiIO` seam) and modes/command-line sections.
  `doc/oracle_roadmap.md` and `doc/oracle_todo.md`, which had drifted into duplicating
  and sometimes disagreeing with each other, were split by role — roadmap owns
  long-horizon phases/ordering/vision, todo owns actionable near-term checkboxes — with
  stale paths, the `<30` vs `35`-line contradiction, and completed-work narratives
  (now just linked to this changelog) removed. `README.md` synced similarly (TUI is
  working, not planned; AI list matches the real `A1`–`A11` agent scheme).
- No game-logic changes; `-a -p` output still matches `bin/expectedresults.txt`.
  `make test_combo` (20/20), `test_recall` (10/10), `test_cash_exchange` (6/6), and
  `test_stda_auto` all pass; `make clean` now leaves the tree free of `.o` files.

## 2026-07-26 — Hidden `--oracle-complete` option + bash tab-completion script

- Added a hidden `--oracle-complete[=WHAT]` option to `parse_options()`
  (`src/main/cmdline.c`) that dumps completion candidates one per line and exits
  cleanly (return `-1`, same as `-h`/`-V`) — deliberately absent from `print_usage()`,
  not part of the public CLI. Bare `--oracle-complete` lists every option spelling
  (all 3 forms where they exist, `-`/`--` dashed per `print_usage()`'s convention,
  trailing `=` when the option takes an argument); `=agents` lists the `-A`/`--ai`
  shorthand codes (via new `print_ai_agent_shorthand_codes()` in
  `src/ui/shared/player_config.c`/`.h`, a bare-codes counterpart to the existing
  localized `print_ai_agent_shorthand_list()`); `=langs` lists the `-u`/`--ui.lang`
  codes (`en`/`fr`/`es`). `long_options[]` moved from a `parse_options()`-local
  `static` array to file scope so `print_completion_list()` can also read it.
- Added `tools/oracle-completion.bash`, a bash completion script (source it manually,
  e.g. from `~/.bashrc`) that calls the binary for every candidate list instead of
  hardcoding any of them, so adding a new option/agent/language needs no script
  change. Handles bash's default `COMP_WORDBREAKS` splitting `--ai=rand`-style
  arguments at `=`, the attached-only `-A<agent>` form, and file completion for
  `-i`/`-o`.
- No game-logic changes; `-a -p` output still matches `bin/expectedresults.txt`.

## 2026-07-24 — Removed `oldsrc/`; `make format` now excludes `ideas/`

- **Removed `oldsrc/`** (pre-refactor implementation) and the `oldcode`/`make oldcode`
  Makefile target, per the `oracle_todo.md` "Code Cleanup" item -- everything in it is
  fully preserved in git history, so it had no reason to keep living in the working
  tree. `OLDSRCDIR`/`OLDBUILDDIR` variables and the `oldobj/%.o` build rule removed
  along with it. (The file deletions themselves ended up bundled into the prior
  commit alongside the TUI polish pass -- a staging mix-up, not intentional -- but
  the removal is recorded here where it belongs.)
- **`make format`** now passes `--exclude=ideas` to `astyle` so the recursive
  `--project` sweep no longer reformats `ideas/`'s design-exploration/prototype code
  (which deliberately doesn't follow the real codebase's conventions -- see
  `CLAUDE.md`'s note on that folder).

## 2026-07-24 — TUI polish pass: layout, colors, card formatting, playability fixes

Hands-on play of the Milestone 2 human-vs-AI `stda.tui` surfaced ~17 UI/layout/
playability rough edges; all fixed in one pass, grouped low-risk to higher-risk. Two
items were deliberately deferred (see "Left for a future pass" below).

- **Gameplay fixes**:
  - Removed the forced "press any key" pause after combat resolves
    (`tui_play_turn_with_humans()`, `stda_tui_interactive.c`) — safe now that combat/
    damage/energy persist in the Game Messages box instead of scrolling past in the
    Console.
  - Fixed the Active/Waiting status-bar label to key off `gstate->player_to_move`
    (the player whose decision is actually pending) instead of `current_player` (this
    turn's fixed attacker) — previously a human *defender* showed as "Waiting" while
    the AI attacker showed "Active". Found and fixed a follow-on bug during tmux
    playtesting: `attack_phase()`/its human mirror always point `player_to_move` at
    the defender, even when no champions were played or after combat already
    resolved, so the attacker's own end-of-turn housekeeping (luna collection,
    discard-to-7) was showing the *defender* as wrongly Active — fixed by resetting
    `player_to_move` back to the attacker right after the attack/defense/combat block.
- **Info-column text**: `q=quit` added to the Shortcuts box (was missing); `P=pass`
  lowercased to `p=pass` display-wide (parsing already accepted both cases);
  `Enter=play` now wraps onto its own line (`tui_print_wrapped()` honors an explicit
  `\n`); the game-start Console line is now just "Game started." for human games
  (full hints live in the always-visible Shortcuts box).
- **Bottom-row layout restructure**: the command line now shares the bottom row with
  Player A's status bar (table portion for the bar, info-column portion for the
  command line) instead of its own full-width row — reclaims a body row. The
  PLAY-mode status line shrank to just the staged-index list (`PLAY [1,2]`) to fit
  the narrower command window; full key hints live in the Shortcuts box.
- **Play-area labels/positioning**: hand labels dropped the redundant player name
  (`Hand (6)` / `Hand (5) [Hidden]` — the status bars already say whose hand it is);
  `Deck`/`Discard` counts moved from one combined centered label to separate
  corner labels, each tucked right where that pile's actual content starts/grows
  (Discard top-left / Deck top-right for Player B, mirrored for Player A); the
  `Combat zone PLAYER x (n):` labels were removed (table side + the `-- combat zone --`
  divider already make ownership obvious) and champions now render as a vertical
  stack tucked directly against the divider, growing toward the owning player's hand
  as more are added.
- **Game Messages box now does something**: added a second message ring buffer
  (`tui_add_game_message()`/`_colored()`, mirroring the existing Console
  `tui_add_message()` pair) and a shared `tui_draw_message_pane()` renderer for both
  boxes. Routing: all narrative (turn summaries, combat resolution/damage/energy,
  action outcomes like "Played N champion(s)") now goes to Game Messages; Console is
  reserved for interaction (prompts, input echo, validation errors, recall/cash
  candidate lists). The box itself was previously decorative — drawn but never
  populated. Also made it (and the Shortcuts box) genuinely tall: both now split the
  info column's remaining height (after Shortcuts) evenly, instead of Console getting
  whatever was left over.
- **Card formatting — hybrid, localized, colored**: `tui_format_card()` (compact
  board form, used in the discard grid) no longer hardcodes French `Pig`/`Rap`/
  `Echange` labels regardless of UI language — draw/recall/cash labels now go
  through `LOCALIZED_STRING` like everywhere else. New detailed CLI-style form
  (`tui_format_card_detailed()`/`tui_draw_card_detailed()`, mirroring
  `cli_display.c`'s `display_player_hand()`) used for the hand and combat zone —
  roomier areas, so full species name + `(D+, L)` breakdown instead of the compact
  abbreviated form; both the compact and detailed forms now color the luna cost cyan
  and the card's own name/label in its type color (champion color, green for draw,
  dim for cash) — previously the whole compact string was a single flat color.
  Hand-card index prefixes (`[1] ...`) gained a space after the bracket.
- **Player colors borrowed from the CLI**: top/bottom status bars switched from
  plain red/green to the CLI's bold cyan (Player A) / bold yellow (Player B)
  (`cli_display.h`'s `COLOR_P1`/`COLOR_P2`); player-attributed Game Messages lines
  (attacker/defender headers, energy-change lines) now use the same two colors via a
  new `tui_player_msg_color()` helper.
- **Left for a future pass** (deliberately out of scope this time, see the approved
  plan): moving the pre-ncurses player-setup questions (mode/name/AI-strategy prompts,
  currently plain stdio before `initscr()`) into the Console box — larger scope,
  touches CLI-shared setup code, planned as its own milestone; rendering deck-card
  contents (only meaningful once a card-visibility model exists for "discard shuffled
  back into deck" — not yet modeled).
- Verified: `-a -p` regression identical throughout every incremental step; multiple
  `tmux`-scripted human-vs-AI sessions (including escape-sequence capture to confirm
  actual ANSI colors, not just layout) in both English and French confirmed every fix
  end-to-end — corner labels, combat-zone stacking/divider-tucking, Game-Messages
  vs. Console routing, no more forced pause, correct Active/Waiting through a full
  attack→defense→discard-to-7 cycle, and the localized card labels in both languages.

## 2026-07-23 — TUI Milestone 2, Passes 2 & 3: playable human-vs-AI TUI

Builds on Pass 1's `UiIO` seam to deliver a fully playable human-vs-AI `stda.tui`:
attack/defense/recall/cash-exchange/mulligan/discard-to-7, both a `TAB`-toggled
COMMAND-mode line editor and a PLAY-mode digit-staging flow for champion selection,
live combat-result display, and context-sensitive shortcuts text.

- **New `src/ui/tui/tui_input.c/h`**: the TUI's `UiIO` backend. `message` maps
  `UiMsgKind` to color-tagged console lines (new `tui_add_message_colored()` /
  `TUI_MSG_COLOR_*` in `tui_render.h`); `read_line` is a `getch()`-based line editor
  drawn into the command window (`tui_draw_command_line()`); `show_card_list` formats
  recall/cash-exchange candidates via the newly-exported `tui_format_card()`.
- **`src/roles/stda/stda_tui.c`**: now runs the same pre-ncurses player-configuration
  menu as `stda_cli.c` (`display_player_selection_menu`/`get_player_names`/
  `get_ai_strategies`/`get_player_assignment`, all before `tui_screen_create()`),
  then a human-vs-AI-aware game loop. **New `src/roles/stda/stda_tui_interactive.c/h`**
  holds the human-turn handlers: `tui_handle_interactive_attack`/`_defense` (PLAY-mode
  digit-staging -- 1-9 toggles a hand card, Enter plays the staged set via
  `validate_and_play_champions()`, Esc clears, `P` passes, `TAB` drops into full
  COMMAND-mode line editing for draw/cash/recall/exit), `tui_handle_interactive_mulligan`/
  `_discard_to_7` (COMMAND-mode only), and `tui_play_turn_with_humans()` (phase-by-phase
  orchestrator mirroring `cli_game.c`'s `execute_game_turn()`, mixing human handlers and
  plain AI strategy calls per phase per player type). AI-vs-AI games still take the
  original M1 `play_turn()` fast path unchanged.
- **New `src/ui/tui/tui_render_playarea.c` and `tui_render_io.c`**: `tui_render.c` was
  split three ways (mirroring the `cli_display.c`/`cli_action_display.c` precedent) to
  stay under the file-size guideline after this milestone's additions -- board/hand/
  discard/combat-zone drawing moved to `_playarea.c`; the message log, input predicates,
  command-line drawing, and combat-details rendering (`tui_show_combat_details()`,
  TUI's equivalent of `display_combat_details_cli()`) moved to `_io.c`.
- **Shared mulligan/discard-to-7 grammar**: `game_process_mulligan_command()` /
  `game_process_discard_command()` added to `ui/interactive/game_commands.c`
  (mirroring the attack/defense split from Pass 1); `cli_game.c`'s
  `process_mulligan_command`/`process_discard_command` are now thin wrappers
  (CLI-only `help` interception, then delegate) -- same pattern as `cli_input.c`.
- **Bug found and fixed, both in `tui_render.c`**:
  1. **Blank-screen hang**: with the player-config menu now running its own
     `printf`/`fgets` prompts before `tui_screen_create()`, the very first
     `wrefresh()` on any of the independent `newwin()`-created panels became a
     silent no-op (returned OK, wrote nothing) because `stdscr` itself is never
     drawn to or refreshed and ncurses' physical/virtual screen sync was never
     seeded. Fixed with `fflush(stdout)` + one plain `refresh()` right after
     `initscr()`/color setup, before any panel is ever refreshed. (M1 never hit
     this because it called `initscr()` immediately, with no prior stdio output.)
  2. **Attacker/Defender status-bar labels inverted mid-turn**: `tui_role_label()`
     keyed off `gstate->turn_phase`, which `attack_phase()` (AI path) flips to
     `DEFENSE` partway through -- fine for M1 (only ever redrew once, after a full
     `play_turn()`, when `turn_phase` was always stale-`DEFENSE` in a way that
     happened to cancel out) but wrong once a human is actually watching mid-turn.
     Fixed by keying the label purely off `current_player` (always this turn's
     attacker until `end_of_turn()`), which needs no `turn_phase` reference at
     all. Also made `tui_handle_interactive_attack()` set
     `turn_phase = DEFENSE`/`player_to_move` itself (mirroring what
     `attack_phase()` does for AI), so the shortcuts-panel hint text stays
     correct in a Human-vs-Human game too, not just Human-vs-AI.
- Verified: `-a -p` regression identical; `test_recall`/`test_cash_exchange`/
  `test_combo` still 10/10, 6/6, 20/20; a full `tmux`-scripted human-vs-AI game
  (attack via PLAY-mode staging, AI auto-defense, combat display, AI attack,
  human defense via PLAY-mode staging, second combat display, COMMAND-mode
  `draw` command, graceful `q` quit) played correctly end-to-end; `tmux`-scripted
  valgrind pass on the same flow: 0 errors, 0 definitely/indirectly-lost bytes
  (same ncurses/terminfo "still reachable" pattern as M1's prior valgrind checks).
- Not yet done from the M2 handout (left for a future pass): visual highlighting of
  staged cards directly in the hand display (currently shown as a `[n,m]` list in the
  command-line row instead); a help overlay; TUI↔SIM mode switching (low priority,
  `stda.sim` doesn't exist yet either).

## 2026-07-23 — TUI Milestone 2, Pass 1: shared interactive command seam (`UiIO`)

Behavior-preserving refactor, no user-visible change yet -- lays the groundwork so
Milestone 2's human-vs-AI TUI can reuse the CLI's interactive rules instead of
duplicating them (see the "TUI Mode" section of `doc/oracle_todo.md`).

- **New `src/ui/shared/ui_io.h`**: a small `UiIO` function-pointer struct
  (`message`/`read_line`/`show_card_list`) that decouples the interactive command
  grammar from stdio. Board/state rendering is explicitly NOT part of this seam --
  each UI keeps rendering its own way (`cli_display.c` vs `tui_render.c`); only the
  three points where the shared rules used to touch stdio directly (feedback
  messages, blocking line reads, "show this titled card list") go through it.
- **New `src/ui/interactive/game_commands.c` + `game_commands_cards.c`**: the
  UI-agnostic command grammar and rules moved out of `ui/cli/cli_input.c` --
  attack/defense dispatch (`cham`/`draw`/`cash`/`pass`/`exit`), champion-play
  validation, and (in the `_cards.c` split, mirroring the `cli_display.c`/
  `cli_action_display.c` precedent) recall (draw/recall cards, exact-count) and
  cash exchange. Each function now takes a `UiIO*` instead of calling
  `printf`/`fgets` directly.
- **New `src/ui/cli/cli_io.c/h`**: the CLI's `UiIO` backend -- `message` maps to
  the existing ANSI color scheme, `read_line` to `fgets`, `show_card_list` to
  `display_card_with_power()` (reusing `select_champion_for_cash_exchange()` for
  the cash-exchange "suggested" marker instead of re-deriving it).
  `src/ui/cli/cli_input.c` is now a thin wrapper: it intercepts the CLI-only
  diagnostic commands (`gmst`/`shod`/`help`, which dump the full board/discard/help
  text and have no TUI equivalent yet) and delegates everything else to the shared
  grammar.
- **Relocated `ui/cli/cli_constants.h` to `src/ui/shared/ui_constants.h`**: it was
  already reached into from `ui/shared/player_config.c`/`player_selection.c`
  (a pre-existing sign it was misplaced), and the new shared `game_commands.c`
  needed it too.
- Verified: `-a -p` regression identical to `bin/expectedresults.txt`;
  `test_recall`/`test_cash_exchange`/`test_combo` still 10/10, 6/6, 20/20; all four
  `testsrc/cli_scripts/` canned scripts (recall, cash exchange, combat, discard)
  replayed with unchanged output; valgrind clean (0 leaks/errors) on the recall path.
- Next: Pass 2 wires a TUI `UiIO` backend (`ui/tui/tui_input.c`, `read_line` as a
  `getch()` line editor in the command window) and a human-turn branch in
  `stda_tui.c` for a command-mode-only playable human-vs-AI game.

## 2026-07-14 — TUI layout: shortcuts hint moved, vertical hand, discard corners

Further Milestone 1 polish.

- **Moved the "TAB to toggle play/command modes" hint into the Shortcuts panel**
  (merged with the existing "(context sensitive - M2)" note, wrapped via new
  `tui_print_wrapped()`); `win_command` is now just a bare `> ` prompt.
- **Player A's hand is now a vertical stack** (`tui_draw_hand_vertical()`, one card
  per row, matching the target PDF) instead of the horizontal wrapping row used
  elsewhere. All entries share one x position (centered on the widest entry) so the
  stack reads as a clean column instead of each line being independently (and
  raggedly) centered.
- **Each player's full discard pile now renders as a compact card grid**, growing
  from one corner of the table toward the vertical middle: Player B's grows down
  from the top-left (respecting the blank separator below its status bar), adding
  a new column to the right once a column fills; Player A's mirrors this exactly
  from the bottom-right corner, growing up, adding columns to the left. New shared
  `tui_draw_discard_column()` handles both directions via signed row/column steps,
  with a safety clamp so columns stop before crossing into the centered hand/deck
  /combat-zone content in the middle. Verified up to a 17-card / 2-column pile (B)
  and 14-card / 2-column pile (A) via `tmux`, both totals matching exactly.
- **Corrected an oversized assumption**: hand-related buffers/loops assumed up to
  10-12 cards; the game rule (`discard_to_7_cards()`, called every `end_of_turn()`)
  actually caps hand at 7, and M1 only ever renders after a full turn completes
  (never mid-turn) -- so 7 is a real, not defensive, bound. Tightened
  `tui_draw_hand()`/`tui_draw_hand_vertical()`'s arrays and loop caps from
  12 to 7 accordingly.
- Verified: `-a -p` regression identical; `test_recall`/`test_cash_exchange`/
  `test_combo` still 10/10, 6/6, 20/20; `tmux`-driven valgrind pass (0
  definitely-lost, same ncurses/terminfo pattern as before).
- **Known gap, discussed but not yet addressed**: `stda_tui.c` calls `play_turn()`
  in full per keypress, and `resolve_combat()` clears both combat zones before that
  call returns -- so `gstate->combat_zone` is always empty at draw time, meaning
  `tui_draw_combat_zone()`'s card-rendering path (as opposed to its "(0):" empty
  case) is not exercised by normal AI-vs-AI play under M1. Real coverage needs
  either a one-off synthetic/manual check or Milestone 2's finer-grained
  per-phase advancement (which would naturally pause after `attack_phase()`
  /`defense_phase()` while combat zones are populated).

## 2026-07-14 — TUI layout: mirrored status bars, combat-zone clustering, console wrap

Further Milestone 1 polish, still before starting Milestone 2.

- **Status bars now mirror across the screen's horizontal center line.** Player
  name is centered within the play-area ("table") width (previously the whole
  status line was left-jammed against column 0 of the full-width window, ignoring
  the info column alongside it); lunas/energy sit on the left edge of the table for
  both bars, status/role on the right edge for both bars (Player B's top bar
  previously had status/role on the left and lunas/energy on the right -- the
  opposite of the bottom bar). New shared helpers `tui_print_centered()` /
  `tui_print_3segment()` (moved into a new "Layout helpers" section, used by both
  the status bars and the play-area code) compute position from `pane_width`
  (`getmaxx(win_play)`), not the status window's own full-terminal width.
- **Both players' combat zones now cluster near the vertical middle**, next to the
  `-- combat zone --` divider, with hand/deck/discard pushed to the outer edges
  (near each player's own status bar) -- previously Player B's combat zone sat
  right under its hand/deck near the top, leaving a large blank gap before the
  divider, while Player A's deck/discard/hand sat right under its combat zone near
  the middle, leaving a large blank gap before the bottom status bar (backwards
  from what was intended).
- **One blank separator row** now sits between each status bar and the block next
  to it (top: below Player B's bar; bottom: above Player A's bar). Caught and fixed
  a bug during verification: the first pass put Player A's blank row in the middle
  of the reserved bottom block instead of as the very last row adjacent to the
  status bar -- a scripted `tmux` comparison against Player B's (correctly
  positioned) separator caught the asymmetry.
- **Console messages now wrap instead of truncating.** New
  `tui_build_console_segments()` wraps the most recent messages (bounded lookback)
  into fixed-width segments in chronological order; the display then takes just the
  tail segments that fit the console's height, same "recent window" logic as
  before but at wrapped-line granularity instead of raw-message granularity.
- Verified via `tmux` at 140x45 and the user's actual 281x65 Konsole size, plus the
  full regression/test/valgrind pass (identical `-a -p`, 10/10, 6/6, 20/20, 0
  definitely-lost).
- **Watch item**: `tui_render.c` is now 602 lines, over the 500-line soft limit (not
  the 1000-line firm one). Deferred splitting it while the layout is still being
  actively iterated on (per `cli_display.c`'s precedent, split once feature work in
  this file settles rather than mid-iteration).

## 2026-07-14 — `-h` usage text: added an Examples section

`print_usage()` (`src/main/cmdline.c`) now ends with 3 real-world usage examples (the
most common invocations so far): `-a -p` (automated AI-vs-AI, fixed seed), `-l -u=fr`
(interactive CLI, French UI), `-t -u=fr` (TUI, French UI). Along the way, confirmed
`-u=fr` (short option with `=`) actually works: `getopt_long_only` matches single-letter
names against the long-options table too (`"u"` is registered there), so it splits on
`=` the same way `--ui.lang=fr` does — not just a short-option-attached quirk.

## 2026-07-14 — `-A`/`--ai` now lists AI-agent shorthands instead of erroring

`./oracle -A` previously required an argument via getopt and just threw an unhelpful
getopt error if omitted. Now:

- `-A`/`--ai` takes an **optional** argument. Bare `-A` (or `--ai`) prints the list of
  11 agent shorthands (same roster as the CLI's `display_ai_strategy_menu()`) and exits
  cleanly (exit 0); an unrecognized value (`-Afoo`) prints an error plus the same list
  and exits with failure (exit 1); a valid shorthand proceeds to `MODE_CLIENT_AI` as
  before (still an unimplemented stub).
- New shorthand table (lowercase, letters/digits, <=10 chars each), case-insensitive
  matching: `rand`, `value`, `greedy`, `combo`/`borealis` (two aliases for the same
  agent, A4), `balanced`, `heuristic`, `hbt`, `hbt2ply`, `simplemc`, `ismcts`,
  `ismctsnn`. Implemented as `parse_ai_strategy_shorthand()` /
  `print_ai_agent_shorthand_list()` in `src/ui/shared/player_config.c/h` (reusing the
  existing `AIStrategyType` enum and `get_strategy_display_name()` rather than
  duplicating a second list), called from `src/main/cmdline.c`'s `case 'A':`.
- `print_usage()`'s `-A` entry now matches the `=[VALUE]` convention already used by
  `-u`/`-p` (optional args) and `-i`/`-o` (required args), plus an explicit note that the
  argument must be attached (`-Afoo`/`--ai=foo`), not space-separated — same
  getopt-driven limitation `-u`/`-p` already have, just undocumented there.
- Verified: all four combinations (bare, valid, invalid, both short/long forms) behave
  as designed; `-a -p` regression identical; `test_recall`/`test_cash_exchange`/
  `test_combo` still 10/10, 6/6, 20/20.

## 2026-07-14 — TUI layout: centered play-area content ("table" feel)

Follow-up polish on Milestone 1 before starting Milestone 2. The play area previously
left-justified every label and card row at column 1 of `win_play`, so on any terminal
wider than the bare minimum the whole right side of the play area was empty space —
didn't read as a card table the way the target PDF/xlsx layout does.

- `src/ui/tui/tui_render.c`: added `tui_print_centered()` (single-line labels) and
  `tui_draw_card_row()` (a shared, wrapping, per-row-centered layout for both
  `tui_draw_hand()`'s and `tui_draw_combat_zone()`'s card lists, via a small `TuiCardCell`
  struct so both call sites build pre-formatted cells and hand them to one layout
  routine instead of duplicating the wrap/measure logic). Hand/combat-zone headers,
  deck/discard counts, and the `-- combat zone --` divider are now all horizontally
  centered in the play window; card rows are centered as a block per row too.
- Verified visually via `tmux` (now installed) at several sizes, including the practical
  minimum (100x30) and the user's actual full-screen Konsole size (281x65) — confirmed
  the "please enlarge" fallback and the 100x30 minimum both work correctly (an earlier
  live-resize report of needing 143x43 turned out to be Konsole not having reached
  100x30 yet mid-drag, not a bug).
- Re-verified: `-a -p` regression identical, `test_recall`/`test_cash_exchange`/
  `test_combo` still 10/10, 6/6, 20/20, and a `tmux`-driven valgrind pass (0
  definitely-lost, same ncurses/terminfo "possibly lost" pattern as Milestone 1).

## 2026-07-14 — TUI mode Milestone 1 (ncurses display skeleton)

`stda.tui` (`-t`/`--stda.tui`) is real: an ncurses text UI matching the target layout in
`Template TUI Game Interface.pdf`/`Gabarit Interface de Jeu Version Texte.xlsx` (2/3 play
area + 1/3 info column, mirrored top/bottom status bars, scrolling console). Milestone 1
is **display-only** — AI-vs-AI, one turn advances per keypress, no human interaction yet
(that's Milestone 2, see `doc/oracle_todo.md`).

- New `src/ui/tui/tui_render.c/h`: all ncurses window layout + drawing, fully responsive
  (`tui_layout()` recomputes every window from the live terminal size, handles
  `KEY_RESIZE`, shows a "please enlarge terminal" fallback below `TUI_MIN_COLS`x
  `TUI_MIN_ROWS` (100x30) and recovers cleanly once resized back up).
- New `src/roles/stda/stda_tui.c/h`: the real `run_mode_stda_tui()`, reusing
  `initialize_cli_game()`/`cleanup_cli_game()`/`apply_mulligan()` and driving `play_turn()`
  once per keypress; `q`/`Q` quits.
- `makefile`: added `-lncursesw` to `LIBS`.
- `game_constants.c/h`: added `CHAMPION_SPECIES_ABBR[]` (3-letter card labels), matching
  `CHAMPION_SPECIES_NAMES`'s existing English-only convention (species names aren't
  localized elsewhere in the codebase either).
- **Two real bugs found via testing, both fixed**: (1) the top status bar duplicated the
  PDF mockup's literal "Actif / En attente" header text instead of resolving it to a
  single computed label per player (copy-paste artifact caught by a scripted PTY
  walkthrough); (2) `setup_game()` never initializes `gstate->turn_phase`/
  `player_to_move` (only `begin_of_turn()` does) — the CLI never notices because it always
  runs `begin_of_turn()` before displaying anything, but the TUI draws once before the
  first `play_turn()` call, so `stda_tui.c`'s setup now sets both fields explicitly
  (caught by valgrind as an uninitialized-value read).
- **ncurses/`ChampionColor` naming collision** (`COLOR_RED` is both an ncurses macro and
  this codebase's own enum constant): `tui_render.h` never includes `<ncurses.h>` (forward
  -declares `WINDOW` as opaque); `tui_render.c` includes `game_types.h` first, then
  `<ncurses.h>`, then `#undef COLOR_RED`, using its own `NC_RED`/etc. constants for
  `init_pair()`. See `CLAUDE.md`'s "Known architectural gaps" for the durable note.
- Also fixed a related loop bug: pressing a key while the terminal was below the minimum
  size used to silently advance a game turn with nothing visible to show for it; now
  ignored until resized back up.

Verified via `make clean && make` (no new warnings), `./bin/oracle -a -p` regression
(identical), `make test_recall`/`test_cash_exchange`/`test_combo` (10/10, 6/6, 20/20), a
scripted PTY walkthrough (`python3` + the `pty` module) driving `stda.tui` through several
turns, a resize up/down/below-minimum/recovery cycle, and FR localization, plus a
valgrind pass (0 definitely-lost bytes; the only "possibly lost" blocks trace entirely
into `libncursesw`/`libtinfo` terminfo internals, a well-known false-positive pattern, not
Oracle's own code).

## 2026-07-14 — Source folder structure cleanup (pragmatic pass)

Pragmatic pass only (not the full v4 engine rewrite) — see
`ideas/done/1 improve source code folder structure/pragmatic_cleanup_implementation_plan.md`
for full detail.

- Split `cli_display.c` (576 lines) into `cli_display.c` (233 lines, core status/turn
  display) + new `cli_action_display.c` (357 lines, action-flow/card-selection display)
  — both now under the 500-line soft limit.
- Fixed `make test_combo`: stale include paths, stale Makefile paths, removed a test of
  the since-removed `get_order_from_species()`, and fixed a latent test bug where
  `CombatCard` literals left the (later-added) `.order` field zero, causing spurious
  order-match bonuses — now 20/20 passing.
- Doc sync: `CLAUDE.md` module layout / file-size / test-status notes updated.

Verified via `make clean && make` (no new warnings), `./bin/oracle -a -p` regression
(identical to `bin/expectedresults.txt`), `make test_recall`/`test_cash_exchange`/
`test_combo` (10/10, 6/6, 20/20), `testsrc/cli_scripts/` re-run, and a full valgrind pass
(0 errors/0 leaks, auto + interactive).

## 2026-07-14 — CLI AI-strategy menu synced with planned agent roster

`display_ai_strategy_menu()`/`get_ai_strategy_choice()`/`get_strategy_display_name()` in
`src/ui/shared/player_config.c` and the `AIStrategyType` enum now list all 11 planned
agents (`A1`-`A11`, skipping `A2` since parameter storing/optimization is calibration
tooling, not an agent) as "not yet implemented" stub menu entries, in `ideas/A#` order,
each with a comment cross-referencing its `ideas/A#` folder. `A4`'s menu entry is
explicitly labeled "Combo Aware [Borealis benchmark]". The former "Hybrid" entry is
confirmed to be `A7` (tactical+HBT: Heuristics+Balanced+Tactical) and is now labeled
"Hybrid (HBT)".

## 2026-07-14 — `ideas/` folder renumbering

Folders were renumbered twice in one session: first to flatten decimal numbers (`12.1`,
`14.3`, etc.) into plain integers, then to pull all AI-agent folders into their own
`A1`-`A11` namespace (kept in their existing relative order) so adding new AI ideas
doesn't require renumbering everything else. `ismcts_nn_overview.md` became its own
folder (`A11`) since it also covers the NN+MCTS extension, distinct from plain IS-MCTS
(`A10`). See `git log` if an old number (e.g. `ideas/8/`, `ideas/14.3/`) shows up in an
older doc or commit message.

## 2026-07-13 — Turn Logic Module: recall, cash exchange, combat display, discard display

Complete Turn Logic Module: full game loop working end-to-end in interactive mode with
all the rules.

- **Display Discard Pile in CLI Mode** — `gmst` (summary) and `shod` (detailed,
  power-sorted) commands; see `ideas/done/4 ...`.
- **Recall Card functionality in stda.cli mode** — recall is **exact and mandatory** (a
  "recall 1 / draw 2" card recalls exactly 1 champion, "recall 2 / draw 3" recalls
  exactly 2; recall is only offered when discard holds enough champions). The Random AI
  engine still only ever draws (never recalls), which is fine given it's not meant to be
  strong. See `ideas/done/2 ...`, `doc/game_rules_doc.md` (recall section corrected to
  match), and `testsrc/test_recall.c`. Implementation: `validate_and_recall_champions()`
  + `handle_recall_choice()` in `cli_input.c`, UI via `display_recallable_champions()`.
- **Combat results display in stda.cli mode** — per-champion rolls/base/combo/damage
  breakdown, shown whenever a human is involved; `stda.auto` unaffected. See
  `ideas/done/3 ...`. Implementation: `display_combat_details_cli()` in
  `ui/cli/cli_display.c` (now `cli_action_display.c` after the 2026-07-14 split).
- **Cash card champion selection in interactive mode** — ask user to select the champion
  card to exchange instead of the AI power-heuristic auto-pick; interactive path
  (`play_cash_card_interactive`) lets the human pick freely. Along the way, fixed a real
  bug in the AI heuristic (`select_champion_for_cash_exchange` conflated "not found"
  with card index 0, a valid champion, using it as a sentinel — now uses `UINT8_MAX`).
  See `ideas/done/5 ...` and `testsrc/test_cash_exchange.c`.

**Note**: fixing the index-0 sentinel bug changed `stda_auto`'s RNG-dependent play
sequence (different AI hand state whenever that bug used to fire), so
`bin/expectedresults.txt` was regenerated (2026-07-13) to reflect the corrected behavior
— this was a deliberate re-baseline, not a regression.

All four verified via `make test_recall` / `make test_cash_exchange`, the
`testsrc/cli_scripts/` manual scripts, a full valgrind pass (auto + interactive), and the
`./bin/oracle -a -p` regression check against the regenerated `bin/expectedresults.txt`.
