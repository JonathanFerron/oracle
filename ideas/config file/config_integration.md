# INI Configuration File Parser - Integration Guide

## Features

- **Comments**: Supports comments with `#` or `;`
- **Sections**: Section headers with `[section]`
- **Key-Value Pairs**: Simple `key = value` syntax
- **Whitespace Handling**: Automatic whitespace trimming
- **Boolean Parsing**: Supports true/false, yes/no, 1/0
- **Quote Removal**: Automatically removes quotes from values
- **Case-Insensitive**: Keys and sections are case-insensitive
- **Error Reporting**: Line number reporting for malformed entries
- **UTF-8 Support**: Handles UTF-8 encoded files

## Example Configuration File

Create a file named `oracle.conf`:

```ini
# Oracle: Les Champions d'Arcadie
# Configuration file

[general]
verbose = true
numsim = 5000

[mode]
# Options: stda.auto, stda.sim, stda.tui, stda.gui, 
#          server, client.sim, client.tui, client.gui, ai
type = stda.auto

[ai]
agent = ISMCTS

[output]
file = results.txt
```

## Integration into Oracle Project

### Step 1: Add to oracle.h

Add the function declaration to `oracle.h`:

```c
/* Configuration file parsing (config.c) */
int read_config_file(const char *filename, config_t *cfg);
```

### Step 2: Modify main.c

In `main.c`, add the config file loading after parsing command line options.

Find this section:
```c
/* Parse command line options */
ret = parse_options(argc, argv, &cfg);
if (ret != 0) {
    cleanup_config(&cfg);
    return (ret < 0) ? 0 : ret;
}
```

Add immediately after:
```c
/* Load config file if specified */
if (cfg.input_file) {
    if (read_config_file(cfg.input_file, &cfg) != 0) {
        cleanup_config(&cfg);
        return 1;
    }
}
```

**Note**: Command-line arguments will override config file settings since they're parsed first.

### Step 3: Compile

Add `config.c` to your compilation:

```bash
gcc -o oracle main.c cmdline.c config.c -std=c11
```

Or with a Makefile:
```makefile
SOURCES = main.c cmdline.c config.c
OBJECTS = $(SOURCES:.c=.o)

oracle: $(OBJECTS)
	$(CC) -o $@ $(OBJECTS)

clean:
	rm -f $(OBJECTS) oracle
```

## Testing

### Standalone Test

Compile the config parser with its test main function:

```bash
gcc -DCONFIG_TEST -o config_test config.c -I. -std=c11
```

Run the test:
```bash
./config_test oracle.conf
```

Expected output:
```
Configuration:
  verbose: true
  numsim: 5000
  mode: 1
  ai_agent: ISMCTS
  output_file: results.txt
```

### Integration Test

Test with the full oracle program:

```bash
# Create a test config file
cat > test.conf << 'EOF'
[general]
verbose = true
numsim = 2000

[mode]
type = stda.sim
EOF

# Run oracle with config file
./oracle --input test.conf
```

### Test with Command-Line Override

Command-line arguments override config file settings:

```bash
# Config file sets numsim=2000, but command line overrides to 3000
./oracle -in test.conf -ns 3000 -vb
```

## Usage Examples

### Example 1: Automated Simulation
```ini
[general]
verbose = false
numsim = 10000

[mode]
type = stda.auto

[output]
file = simulation_results.txt
```

Run with:
```bash
./oracle --input simulation.conf
```

### Example 2: AI Agent Configuration
```ini
[general]
verbose = true

[mode]
type = ai

[ai]
agent = ISMCTS

[output]
file = ai_game.log
```

Run with:
```bash
./oracle -in ai_config.conf
```

### Example 3: Server Mode
```ini
[general]
verbose = true

[mode]
type = server

[output]
file = server.log
```

Run with:
```bash
./oracle --input server.conf
```

## Configuration File Format Reference

### Sections

- `[general]` - General settings
- `[mode]` - Game mode selection
- `[ai]` - AI agent configuration
- `[output]` - Output file settings

### Keys

#### [general] section
- `verbose` - Enable verbose output (boolean)
- `numsim` - Number of simulations (positive integer)

#### [mode] section
- `type` - Game mode (one of: stda.auto, stda.sim, stda.tui, stda.gui, server, client.sim, client.tui, client.gui, ai)

#### [ai] section
- `agent` - AI agent name (string, e.g., ISMCTS)

#### [output] section
- `file` - Output file path (string)

### Value Types

**Boolean values** can be:
- `true`, `false`
- `yes`, `no`
- `1`, `0`

**Strings** can be:
- Unquoted: `value`
- Single-quoted: `'value'`
- Double-quoted: `"value"`

**Comments** start with `#` or `;`

## Error Handling

The parser will:
- Skip empty lines and comments
- Warn about malformed sections (missing `]`)
- Warn about missing `=` in key-value pairs
- Warn about invalid values with line numbers
- Use default values when parsing fails
- Return non-zero on file open errors

## Notes

- Command-line arguments take precedence over config file settings
- Config file settings take precedence over default values
- The parser is case-insensitive for keys, sections, and boolean values
- Whitespace around keys and values is automatically trimmed
- Line length is limited to 256 characters