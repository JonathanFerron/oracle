# Oracle TUI (Text User Interface) Guide

## Overview

The Oracle TUI provides an interactive ncurses-based interface for playing Oracle: Les Champions d'Arcadie. This mode allows human players to play against each other or against AI opponents in a terminal-based graphical interface.

## Building and Running

### Prerequisites

- GCC compiler with C23 support
- ncurses development library
- Make

On Ubuntu/Debian:
```bash
sudo apt-get install libncurses5-dev libncursesw5-dev
```

On macOS:
```bash
brew install ncurses
```

### Building

```bash
make clean
make
```

### Running TUI Mode

```bash
# Launch TUI directly
./bin/oracle -t

# Or use the makefile shortcut
make tui

# With verbose output (writes to log)
./bin/oracle -t -v -o game.log
```

## Interface Layout

The TUI screen is divided into four main areas:

```
┌──────────────────────────────────────────────┬─────────────────────────┐
│                                              │                         │
│  Player B Info                               │    Game Log Console     │
│  Hand: [Hidden]                              │                         │
│  Deck/Discard Info                           │  - Game messages        │
│  Combat Zone                                 │  - Action results       │
│                                              │  - Turn information     │
│  ──────────────────────────────────          │                         │
│                                              │                         │
│  Combat Zone                                 │                         │
│  Deck/Discard Info                           │                         │
│  Hand: [0]d4+0 [1]d6+1 [2]d8+2...           │                         │
│  Player A Info                               │                         │
│                                              │                         │
├──────────────────────────────────────────────┴─────────────────────────┤
│  Status Bar: TAB: Command | 0-9: Play | P: Pass | Q: Quit             │
├────────────────────────────────────────────────────────────────────────┤
│  Command: >                                                            │
└────────────────────────────────────────────────────────────────────────┘
```

### Screen Sections

1. **Game Area (Left)**: Shows both players' game states
   - Player B (opponent) at top
   - Player A (you) at bottom
   - Each showing: status, hand, deck/discard counts, combat zone

2. **Console (Right)**: Scrolling log of game events

3. **Status Bar**: Context-sensitive help and current mode

4. **Command Line**: Text input for commands

## Controls

### Play Mode (Default)

- **0-9**: Play card at that index from your hand
- **P**: Pass your turn
- **Q**: Quit the game
- **TAB**: Switch to command mode

### Command Mode

- **TAB**: Return to play mode
- **Type commands** and press **ENTER**

## Commands

Available in command mode:

- `help` - Show available commands
- `pass` - Pass your turn
- `quit` - Exit the game
- More commands coming soon!

## Game Information Display

### Player Info Format
```
Player A | Active/Waiting | Attacker/Defender | 25 lunas | 87 energy
```

### Card Display Format

Champion cards show:
- `[index] dDICE+BASE SPECIES` (e.g., `[0]d4+1 HUM`)
- Color-coded by champion color (red/indigo/orange)

Special cards show:
- `[index] Draw2` or `[index] Draw3` for draw cards
- `[index] Cash` for cash exchange cards

### Combat Zone
Shows cards currently in combat with their stats

### Deck/Discard
Shows count of cards: `Deck: 23 | Discard: 5`

## Color Coding

- **Cyan**: Headers and labels
- **Red**: Red champion cards
- **Blue**: Indigo champion cards
- **Yellow**: Orange champion cards
- **Green**: Status information
- **White**: Default text

## Gameplay Flow

1. **Start**: Game begins with Player A active
2. **Draw**: Current player draws 1 card (except first player, first turn)
3. **Attack Phase**: Active player can:
   - Play champion cards for attack
   - Play draw/recall cards
   - Play cash exchange cards
   - Pass turn
4. **Defense Phase**: If champions were played, opponent can defend
5. **Combat**: Resolve attack vs defense, apply damage
6. **End Turn**: Collect 1 luna, discard to 7 cards, switch players
7. **Win Condition**: First player to reduce opponent to 0 energy wins

## Tips

1. **Card Selection**: Cards are indexed 0-9 in your hand
2. **Resource Management**: Watch your luna (cash) balance
3. **Combo Bonuses**: Play cards of same species/color for bonuses
4. **Defense Strategy**: You don't have to defend every attack
5. **Console Log**: Check the log for detailed combat results

## Limitations (Current Version)

- Single player vs AI only (multiplayer coming soon)
- Limited command set (expanding)
- No save/load functionality yet
- Basic AI opponent (better AI in development)

## Troubleshooting

### Terminal too small
Minimum terminal size: 80x24 characters

### Colors not showing
Ensure your terminal supports 256 colors

### ncurses errors
Make sure ncurses is properly installed:
```bash
ldconfig -p | grep ncurses
```

## Future Enhancements

- [ ] Two-player hot-seat mode
- [ ] Network multiplayer
- [ ] Save/load games
- [ ] Replay mode
- [ ] Enhanced AI opponents
- [ ] Card detail popup
- [ ] Animation effects
- [ ] Sound effects (ASCII beeps)
- [ ] Tournament mode

## Development Notes

The TUI is built using:
- `ncurses` for terminal UI
- Modular C design with clean separation
- Double-buffered rendering
- Message queue for game log
- Strategy pattern for AI

See `src/tui.h` and `src/tui.c` for implementation details.

## Contributing

To extend the TUI:

1. Add new commands in `tui_process_command()`
2. Add new display functions in `tui_draw_*()`
3. Update help text in `tui_draw_status()`
4. Test with various terminal sizes

## License

Part of Oracle: Les Champions d'Arcadie project.
