# Misc UI Ideas — Status

**Renamed from `ideas/3 tui/` (2026-08-20)**: TUI mode itself is now substantially
implemented — Milestones 1 (ncurses display skeleton) and 2 (human-vs-AI play:
`TAB`-toggled PLAY/COMMAND modes, recall, cash exchange, mulligan, discard-to-7, live
combat display) plus a UI/playability polish pass are all done. See `doc/changelog.md`
for the full history and `src/roles/stda/stda_tui.c` + `stda_tui_interactive.c` +
`src/ui/tui/tui_render*.c` + `tui_input.c` for the real implementation. With the TUI
design-exploration content gone (below), what's left here isn't TUI-specific anymore,
hence the rename — it's small, cross-UI polish ideas that don't have another home yet.

Earlier that same day, this folder also had nine early TUI prototype files (a single
flat `tui.c`/`tui.h` design predating the CLI split, pre-reorg flat includes, the old
`HDCLL` linked-list types, and a `cmdline.c`/`main.c`/`makefile` snapshot far behind the
current ones) removed — all fully superseded by the real implementation, deleted
outright rather than archived since none of their code or file layout was still
accurate or still planned.

**What's here now**: `ascii art fonts for logo in tui and cli modes.txt` — a small,
genuinely still-open idea (not implemented anywhere, confirmed by grep) for an ASCII
art logo/banner in TUI and CLI modes, listing candidate figlet font styles.

**Remaining TUI backlog** (not blocking, tracked in `doc/oracle_todo.md`'s "Left for a
future pass", not duplicated here): staged-card highlighting in the hand display, a
help overlay, TUI↔SIM mode switching, moving pre-ncurses player setup into the Console
box, and rendering deck-card contents once a card-visibility model exists.
