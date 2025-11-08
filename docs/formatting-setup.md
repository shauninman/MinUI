# Code Formatting Setup

MinUI uses `clang-format` for consistent code formatting.

## Commands

### Check Formatting (Safe - No Changes)
```bash
make -f Makefile.qa format-check
```

Shows which files would be changed without modifying them.

### Format Code (MODIFIES FILES)
```bash
make -f Makefile.qa format
```

**⚠️ WARNING:** This modifies files in place! Review changes with `git diff` before committing.

### Format Specific File
```bash
clang-format -i workspace/all/minui/minui.c
```

## Configuration

See `.clang-format` for formatting rules:

```yaml
# Indentation - MinUI uses tabs
UseTab: ForIndentation
TabWidth: 4

# Braces - K&R style
BreakBeforeBraces: Linux

# Line length
ColumnLimit: 100

# Pointers
PointerAlignment: Right
```

## Current Status

**The codebase is NOT currently formatted** with clang-format.

This is intentional - we're introducing formatting gradually:

1. **Now:** Tool is available, can format new code
2. **Later:** Format files as they're modified
3. **Much Later:** Format entire codebase (requires review)

## Usage Recommendations

### For New Code
```bash
# After writing new code in file.c
clang-format -i workspace/all/mynewfile.c
```

### For Modified Files
```bash
# After editing existing file
clang-format -i workspace/all/minui/minui.c

# Review changes
git diff workspace/all/minui/minui.c

# If looks good, commit
git add workspace/all/minui/minui.c
```

### DON'T Format Everything Yet

**Don't run** `make -f Makefile.qa format` **on the entire codebase** until:
1. You've reviewed the changes it would make
2. You're ready for a large formatting commit
3. All team members agree

## Why Not Format Everything Now?

1. **Large diff** - Formatting 30,000+ lines creates huge git diff
2. **Git blame** - Makes it harder to see real code changes vs formatting
3. **Gentle approach** - Introduce tools gradually
4. **Review burden** - Need to verify formatting doesn't break anything

## Integration with Editor

### VS Code
Install "C/C++" extension and add to `.vscode/settings.json`:
```json
{
  "editor.formatOnSave": true,
  "C_Cpp.clang_format_style": "file"
}
```

### Vim
```vim
" Format current file
:!clang-format -i %
```

### Emacs
```elisp
(add-hook 'c-mode-hook
  (lambda () (add-hook 'before-save-hook 'clang-format-buffer nil t)))
```

## Formatting Rules Explained

### Tabs vs Spaces
```c
// Uses tabs for indentation (existing MinUI style)
void function(void) {
→   int x = 1;  // → is a tab
}
```

### Pointer Alignment
```c
// Right-aligned (int* p, not int *p)
int *pointer;
char *string;
```

### Brace Style
```c
// Opening brace on same line (K&R/Linux style)
if (condition) {
    doSomething();
}

// Functions
void myFunction(void)
{
    // Opening brace on new line for functions
}
```

### Line Length
```c
// Max 100 characters per line
// Long lines get wrapped automatically
```

## Incremental Adoption

### Phase 1 (Now)
- ✅ Tool installed and configured
- ✅ Can format individual files
- ✅ Documentation written

### Phase 2 (Next Month)
- Format new files as they're created
- Format files as they're significantly modified
- Build muscle memory

### Phase 3 (Later)
- Format entire codebase in one commit
- Add format-check to CI
- Require formatting for all PRs

## Common Issues

### "clang-format not found"
```bash
# macOS
brew install llvm

# Linux
sudo apt-get install clang-format
```

### "Too many changes"
Start with one file at a time:
```bash
clang-format -i workspace/all/clock/clock.c
git diff workspace/all/clock/clock.c
```

### "Don't like the formatting"
Edit `.clang-format` to adjust rules, then test:
```bash
clang-format -i test_file.c
git diff test_file.c
```

## Quick Reference

```bash
# Check what would change (safe)
make -f Makefile.qa format-check

# Format all code (MODIFIES FILES!)
make -f Makefile.qa format

# Format one file
clang-format -i path/to/file.c

# See what would change without modifying
clang-format path/to/file.c | diff path/to/file.c -
```

## Next Steps

1. ✅ **Formatting available** - You are here
2. ⏭️ **Use on new code** - Format files you create/edit
3. ⏭️ **Eventually format all** - When ready for big commit
