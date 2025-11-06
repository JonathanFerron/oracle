# Refactoring Template

## Part 1: New Code

[Complete new files here]

## Part 2: Structural Updates

[File-level changes to existing files]

## Part 3: Patterns

### Pattern 1: [Name]

**Find:** `old_pattern`
**Replace:** `new_pattern`
**Locations:** file.c:function(), including first line number
**Example:**

```diff
- old code
+ new code
```

## Part 4: Cleanup

[Removal instructions]

## 

# Contributing to Oracle Card Game

## Refactoring Methodology

When implementing changes to existing source files, follow this approach:

### Structure

1. **Part 1:** New code creation (complete new files first)
2. **Part 2:** Update existing files to integrate new code
3. **Part 3:** Pattern-by-pattern refactoring with diff-style instructions
4. **Part 4:** Cleanup (file removal, documentation)

### Key Principles

- Pattern-based approach to update the code
- Each step = one pattern across all affected files
- Diff-style instructions with red and green colour coding for code removed and added
- Manual implementation over macro magic (code duplication OK for readability)
- Testing checkpoints after key milestones

# 
