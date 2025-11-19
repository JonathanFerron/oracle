# Oracle: Mulligan and Discard-to-7 Implementation

I'll provide a comprehensive implementation following the project's guidelines and patterns. This will add mulligan and discard-to-7 functionality for the interactive player in CLI mode.

I've provided a complete implementation of mulligan and discard-to-7 functionality for the interactive player in CLI mode. Here's what was delivered:

### Key Features

1. **Mulligan System** (`mull` command)
   
   - Player B can discard up to 2 cards at game start
   - Shows card power values to guide decisions
   - Draws replacement cards automatically
   - Supports `pass` to keep current hand

2. **Discard-to-7 System** (`disc` command)
   
   - Triggered when player has > 7 cards at end of turn
   - Shows exactly how many cards must be discarded
   - Displays power values for informed choices
   - Falls back to AI logic if input errors occur

3. **Shared Helper Functions**
   
   - `display_card_with_index()` - Shows cards with power values
   - `parse_card_indices_cli()` - Parses user input with validation
   - `discard_and_draw()` - Handles discard logic with optional draw

### Design Highlights

- **Code Reuse**: Both systems share common parsing and display logic
- **User-Friendly**: Clear prompts, examples, color-coded output, helpful tips
- **Robust**: Validates input, detects duplicates, handles errors gracefully
- **Compliant**: All functions < 30 lines, file stays under limits
- **Consistent**: Follows existing CLI patterns and project guidelines

### Testing Guidance

The implementation guide includes:

- Comprehensive test checklist for both features
- Usage examples with expected output
- Integration points clearly marked
- Known limitations documented
- Future enhancement suggestions

All code is ready to integrate into your existing `stda_cli.c` file. The complete updated file is provided in the artifacts, maintaining your existing functionality while adding the new features seamlessly.
