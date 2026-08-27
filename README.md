# Oracle: Arcadian Champions of Light

**A fixed-pool strategic dueling card game**

Oracle is an open-source implementation of a fixed-pool strategic dueling card game, paired with ongoing AI development. Players command champions from five mystical Orders of Arcadia, using resource management and combat strategy to reduce their opponent's energy to zero.

## 🎮 Game Features

- **Fast-paced gameplay**: Average 20-minute games (30 rounds)
- **120-card deck**: 102 unique champions across 15 species and 5 Orders
- **Strategic depth**: Combo bonuses, resource management, and tactical decisions
- **Multiple play modes**: 
  - Random deck distribution (maximum variety)
  - Monochrome decks (single-color strategy)
  - Custom deck building (advanced play)
  - Three deck drafting modes (advanced play)
    - Solomon 7x7
    - Draft 12x8
    - Draft 1-2-3

## 🤖 AI Research Focus

This project serves as a testbed for AI development, progressing from simple to sophisticated (12 planned agents, `A1`–`A11` plus the side-exploration `A12`; see `doc/oracle_roadmap.md`):

- ✅ **Random strategy** (baseline, functional)
- ✅ **Value Based** ("The Apprentice", `A1`), **Combo Threshold** ("The Showboat", `A2`), **Borealis** (the Bradley-Terry benchmark, `A3`), **Balanced Rules** ("Bean Counter", `A4`), **Heuristic** ("ε-γ-δ", `A5`), **Tactical** ("Pressure Cooker", `A6`), **Hybrid HBT** ("The Grandmaster", `A7`), **Simple Monte Carlo** ("The Soothsayer", `A8`), **HBT 2-Ply** ("Grandmaster II", `A9`), and **Clairvoyant** (`A8`'s sibling, `A12`) — implemented and calibrated
- 📋 IS-MCTS, IS-MCTS + neural network — designed, not yet implemented

A Bradley-Terry rating system for objective AI strength measurement is implemented (`src/rating/`): every agent gets a rating on a 1–99 scale that is its measured win probability against Borealis, the fixed rating-50 anchor.

## 🛠️ Technical Highlights

- **Clean C architecture**: modular design, functions targeting ≤35 lines
- **Cross-platform**: Linux (primary) and MSYS2 (Windows) support
- **Multiple interfaces**: CLI (working), ncurses TUI (working, human-vs-AI), SDL3 GUI (planned)
- **Network-ready**: client/server architecture designed for multiplayer
- **Testable**: GameContext pattern enables dependency injection

## 🚀 Quick Start

```bash
# Clone the repository
git clone https://github.com/JonathanFerron/oracle.git
cd oracle

# Build the project
make

# Run automated simulation (AI vs AI)
./bin/oracle --stda.auto --numsim=1000

# Play interactively (Human vs AI)
./bin/oracle --stda.cli
```

## 📖 Documentation

- [`doc/game_rules_doc.md`](doc/game_rules_doc.md) - Complete game rules
- [`doc/oracle_design.md`](doc/oracle_design.md) - Technical architecture
- [`doc/oracle_roadmap.md`](doc/oracle_roadmap.md) - Development plan
- [`doc/oracle_todo.md`](doc/oracle_todo.md) - Current task tracking
- [`doc/changelog.md`](doc/changelog.md) - Dated history of completed work

## 🎯 Current Status

**First non-dumb AI strategy** is the active focus (see `doc/oracle_roadmap.md`'s "Next Up").

Working features:

- ✅ Random AI strategy
- ✅ CLI interactive mode (human vs AI, human vs human, AI vs AI)
- ✅ TUI interactive mode (ncurses, human vs AI)
- ✅ Automated simulation
- ✅ Mulligan system for interactive player
- ✅ Discard-to-7 mechanic for interactive player
- ✅ Recall mechanic (draw or recall champions from discard)
- ✅ Interactive cash exchange (choose which champion to exchange)
- ✅ Detailed combat results display (per-champion rolls, combo, damage)
- ✅ Discard pile inspection (`gmst` summary, `shod` detail)

In development:

- ⚠️ AI strategies beyond Random (`A1`–`A11`, see above)
- ⚠️ TUI polish items (staged-card highlighting, help overlay — see `doc/oracle_todo.md`)

## 🤝 Contributing

This is a hobby/research project. Contributions, suggestions, and discussions are welcome! Check the [TODO](doc/oracle_todo.md) for areas needing work.

## 📜 License

GPL v3 - See [LICENSE](LICENSE) for details

## 🌟 Why Oracle?

Oracle combines:

- **Game design exploration** - Testing mechanics and balance
- **AI research** - Practical implementation of search algorithms
- **Software architecture** - Clean patterns and maintainable code
- **Cross-platform development** - Portable C with multiple UI targets

Perfect for those interested in game AI, C programming patterns, or strategic dueling card game design.

---
