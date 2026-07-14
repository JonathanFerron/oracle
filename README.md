# Oracle: Arcadian Champions of Light

**A strategic two-player card game**

Oracle is an open-source card game project combining tactical gameplay with AI development. Players command champions from five mystical Orders of Arcadia, using resource management and combat strategy to reduce their opponent's energy to zero.

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

This project serves as a testbed for AI development, progressing from simple to sophisticated:

- ✅ **Random strategy** (baseline)
- 🚧 **Balanced rules-based AI** (in development)
- 📋 **Heuristic evaluation AI**
- 📋 **Monte Carlo methods AI**
- 📋 **Information Set MCTS AI** (advanced)

Includes a Bradley-Terry rating system for objective AI strength measurement.

## 🛠️ Technical Highlights

- **Clean C architecture**: Modular design with <30 lines per function
- **Cross-platform**: MSYS2 (Windows) and Linux support
- **Multiple interfaces**: CLI (working), ncurses TUI (planned), SDL3 GUI (planned)
- **Network-ready**: Client/server architecture designed for multiplayer
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

## 🎯 Current Status

**Phase 2: Turn Logic Completion** (In Progress)

Working features:

- ✅ Random AI strategy
- ✅ CLI interactive mode
- ✅ Automated simulation
- ✅️ Mulligan system for interactive player
- ✅ Discard-to-7 mechanic for interactive player
- ✅ Recall mechanic (draw or recall champions from discard)
- ✅ Interactive cash exchange (choose which champion to exchange)
- ✅ Detailed combat results display (per-champion rolls, combo, damage)
- ✅ Discard pile inspection (`gmst` summary, `shod` detail)

In development:

- ⚠️ Various AI strategies
- ️️⚠️ TUI mode

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

Perfect for those interested in game AI, C programming patterns, or strategic card game design.

---
