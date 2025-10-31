# Contributing to Oracle Card Game

## Refactoring Methodology

When implementing changes to existing source files, follow this approach:

### Structure

1. **Part 1:** New code creation (complete new files first)
2. **Part 2:** Update existing files to integrate new code
3. **Part 3:** Pattern-by-pattern refactoring with diff-style instructions
4. **Part 4:** Cleanup (file removal, documentation)

### Key Principles

- Pattern-based, not file-based commits
- Each commit = one pattern across all affected files
- Diff-style instructions with `-` and `+` prefixes
- Manual implementation over macro magic (code duplication OK for readability)
- Testing checkpoints after key milestones

### Commit Message Format

```
type(pattern-N): Brief description

Pattern: old_pattern -> new_pattern

Changes:
- Specific change 1
- Specific change 2

Files: file1.c, file2.c
```

See [Memory Management Refactoring](docs/refactoring-examples.md) for detailed example.
