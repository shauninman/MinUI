# ShellCheck Setup

MinUI uses `shellcheck` for shell script linting.

## Running ShellCheck

```bash
make -f Makefile.qa lint-shell
```

Checks 403 shell scripts for common issues.

## What Gets Checked

- `commits.sh` - Release commit history script
- `skeleton/**/*.sh` - Boot scripts, pak launchers
- `workspace/**/*.sh` - Install scripts, platform utilities

**Excluded:**
- `workspace/*/cores/**` - Downloaded libretro cores (not our code)
- `toolchains/**` - External toolchain scripts
- `workspace/m17/boot/output/em_ui.sh` - Contains embedded binary data

## Configuration

See `.shellcheckrc` for suppression rules:

```bash
# Start forgiving - only serious issues
disable=SC2086,SC2034,SC2164,SC2046,SC2006,SC2005,SC2003,SC1090,SC2295
severity=warning
```

### Suppressed Warnings (For Now)

Common issues suppressed to reduce noise:

- **SC2086** - Unquoted variables (too common, fix gradually)
- **SC2164** - Missing `cd ... || exit` (scripts assume cd works)
- **SC2006** - Backticks instead of `$()` (style, not critical)
- **SC2181** - Checking `$?` indirectly (style preference)
- **SC3010/SC3014** - Bash-isms in POSIX sh (many scripts use these)

## Current Findings

**202 scripts with issues** (out of 403 checked)

Common real issues found:
- Missing shebang lines
- POSIX incompatibilities (`[[`, `==`)
- Unquoted `rm -rf` with variables (dangerous!)

## Example Issues

### Issue 1: rm -rf safety
```bash
# Dangerous - could delete / if var is empty
rm -rf "$SDCARD_PATH/$MIYOO_DIR"

# Better
rm -rf "${SDCARD_PATH:?}/${MIYOO_DIR:?}"
```

### Issue 2: POSIX compatibility
```bash
# Bash-specific (won't work in /bin/sh)
if [[ "$A" == "$B" ]]; then

# POSIX-compatible
if [ "$A" = "$B" ]; then
```

### Issue 3: Missing shebang
```bash
# Missing at top of file
#!/bin/sh
```

## Usage Recommendations

### For New Scripts
```bash
# Check as you write
shellcheck mynewscript.sh
```

### For Modified Scripts
```bash
# Before committing
shellcheck skeleton/SYSTEM/miyoomini/paks/MinUI.pak/launch.sh
```

## Gradually Improving

**Current approach:** Forgiving rules, find serious issues

**Future phases:**
1. Enable more rules as scripts are fixed
2. Make shellcheck pass with no suppressions
3. Add to CI (when ready)

## Integration with Editors

### VS Code
Install "ShellCheck" extension - automatic linting in editor

### Vim
```vim
let g:syntastic_sh_shellcheck_args = "-x"
```

### Emacs
```elisp
(add-hook 'sh-mode-hook 'flycheck-mode)
```

## Common Fixes

### Add Shebang
```bash
#!/bin/sh
# or
#!/bin/bash
```

### Quote Variables
```bash
# Before
cd $SOME_PATH

# After
cd "$SOME_PATH"
```

### Use POSIX Syntax
```bash
# Before (bash-only)
if [[ "$VAR" == "value" ]]; then

# After (POSIX)
if [ "$VAR" = "value" ]; then
```

## Next Steps

1. ✅ **ShellCheck available** - You are here
2. ⏭️ **Fix critical issues** - `rm -rf` with unquoted vars
3. ⏭️ **Add shebangs** - Many scripts missing them
4. ⏭️ **Improve over time** - Fix as scripts are modified
