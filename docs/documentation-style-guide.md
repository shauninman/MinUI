# C Code Documentation Style Guide

This guide defines the documentation standards for the MinUI codebase. Following these conventions ensures consistency and helps developers understand the code quickly.

## Philosophy

Good documentation should:
- **Explain WHY, not WHAT** - The code already shows what it does
- **Be concise** - Don't over-explain obvious things
- **Focus on non-obvious behavior** - Edge cases, platform quirks, memory ownership
- **Help maintenance** - Future developers (including yourself) should understand intent

## File-Level Documentation

Every `.c` file should start with a header comment:

```c
/**
 * filename.c - Brief description of module purpose
 *
 * More detailed explanation of what this file provides,
 * its role in the system, and any important context.
 */
```

**Example:**
```c
/**
 * utils.c - Core utility functions for MinUI
 *
 * Provides cross-platform utilities for string manipulation, file I/O,
 * timing, and name processing. This file is shared across all platforms
 * and should not contain platform-specific code.
 */
```

## Function Documentation

### Standard Format

```c
/**
 * Brief one-line description of what the function does.
 *
 * Optional longer explanation. Include:
 * - Edge cases or special behavior
 * - Performance characteristics if relevant
 * - Platform-specific notes
 * - Examples for complex transformations
 *
 * @param param_name Description of parameter and constraints
 * @param buffer Pointer to output buffer (modified in place)
 * @return Description of return value and special values
 *
 * @note Important caveats (e.g., "Not thread-safe")
 * @warning Critical issues (e.g., "Caller must free returned memory")
 */
```

### When to Document Functions

| Function Type | Documentation Required |
|---------------|------------------------|
| Public API functions | **Always** - Full documentation |
| Static helper functions | **If non-obvious** - Brief description |
| Trivial getters/setters | **Optional** - Usually self-documenting |
| Complex algorithms | **Always** - Explain the approach |

### Examples

**Good - Explains behavior and constraints:**
```c
/**
 * Reads entire file into dynamically allocated buffer.
 *
 * Allocates exactly the right amount of memory for the file.
 * Returns NULL if file cannot be opened or memory allocation fails.
 *
 * @param path Path to file
 * @return Pointer to allocated buffer, or NULL on failure
 *
 * @warning Caller must free returned pointer
 */
char* allocFile(const char* path);
```

**Bad - Just repeats the function name:**
```c
/**
 * Allocates a file.
 */
char* allocFile(const char* path);
```

## Inline Comments

### When to Add Inline Comments

✅ **DO comment:**
- Non-obvious algorithms or bit manipulations
- Platform-specific workarounds
- Edge case handling
- Magic numbers (or better: use named constants)
- Buffer overlap safety (strcpy vs memmove)
- Security-critical checks

❌ **DON'T comment:**
- Obvious operations (`i++; // increment i`)
- Code that's already clear from variable names
- Standard patterns everyone knows

### Examples

**Good - Explains WHY:**
```c
// Safe for overlapping buffers (source and dest may point to same memory)
memmove(out_name, tmp, strlen(tmp) + 1);

// Extended to 5 for .doom files (was 3)
if (len > 2 && len <= 5) {
    tmp[0] = '\0';
}

// Hardware doesn't support negative brightness values
if (brightness < 0) {
    brightness = 0;
}
```

**Bad - States the obvious:**
```c
// Set i to 0
i = 0;

// Call strlen
len = strlen(str);

// Return the result
return ret;
```

## Section Headers

For files with multiple logical sections, use section headers:

```c
///////////////////////////////
// Section Name
///////////////////////////////
```

**Example sections:**
- String utilities
- File I/O utilities
- Name processing utilities
- Date/time utilities
- Math utilities

## Special Annotations

Use these consistently throughout the codebase:

### @param Tags
```c
@param path Path to file (input)
@param buffer Pre-allocated buffer (modified in place)
@param size Size of buffer in bytes
```

### @return Tags
```c
@return 0 on success, -1 on error
@return Pointer to allocated memory, or NULL on failure
@return 1 if match found, 0 otherwise
```

### @note Tags (General information)
```c
@note Buffer is not modified if file cannot be opened
@note Uses memmove() for safe overlapping buffer copies
@note Not thread-safe - uses static buffer
```

### @warning Tags (Critical information)
```c
@warning Caller must free returned pointer
@warning Input string is modified in place
@warning Only valid on 32-bit ARM platforms
```

## Platform-Specific Code

When documenting code with platform-specific behavior:

```c
#if defined(__ARM_ARCH) && __ARM_ARCH >= 5 && !defined(__aarch64__)
/**
 * Averages two RGB565 pixel values (ARM assembly version).
 *
 * This optimized assembly version is ~2x faster than C on 32-bit
 * ARM devices. Uses ARM-specific instructions for bit manipulation.
 *
 * @note Only used on 32-bit ARM (ARMv5+), not on ARM64/AArch64
 */
uint32_t average16(uint32_t c1, uint32_t c2) {
    // Assembly implementation
}
#else
/**
 * Averages two RGB565 pixel values (portable C version).
 *
 * Fallback implementation for non-ARM platforms.
 * See ARM version for algorithm explanation.
 */
uint32_t average16(uint32_t c1, uint32_t c2) {
    // C implementation
}
#endif
```

## Examples and Edge Cases

For complex transformations, provide examples:

```c
/**
 * Cleans a ROM path for display in the UI.
 *
 * Examples:
 * - "/mnt/SDCARD/Roms/GB/game.gb" -> "game"
 * - "Super Mario (USA) (v1.2).nes" -> "Super Mario"
 * - "Game [!].p8.png" -> "Game"
 *
 * @param in_name Input path or filename
 * @param out_name Output buffer for cleaned name (min 256 bytes)
 */
void getDisplayName(const char* in_name, char* out_name);
```

## Memory Ownership

Always document who owns allocated memory:

```c
/**
 * @return Pointer to allocated buffer
 * @warning Caller must free returned pointer
 */
char* allocFile(const char* path);

/**
 * @return Pointer to internal static buffer (do not free)
 * @note Not thread-safe - buffer reused on each call
 */
char* getDisplayName(const char* in_name);
```

## Buffer Modification

Clearly indicate when buffers are modified:

```c
/**
 * @param line String to normalize (modified in place)
 */
void normalizeNewline(char* line);

/**
 * @param buffer Pre-allocated buffer to fill
 * @note Buffer is not modified if file cannot be opened
 */
void getFile(const char* path, char* buffer, size_t buffer_size);
```

## Common Documentation Patterns

### String Manipulation Functions
```c
/**
 * Brief description of transformation.
 *
 * Example: "input" -> "output"
 *
 * @param str Input string (modified in place / not modified)
 * @return Success indicator or transformed value
 */
```

### File I/O Functions
```c
/**
 * Reads/Writes data from/to a file.
 *
 * Error behavior: Returns NULL / silently fails / etc.
 *
 * @param path Path to file
 * @return Data or status code
 *
 * @warning Memory ownership / buffer size requirements
 */
```

### Validation Functions
```c
/**
 * Checks if condition is met.
 *
 * Specific conditions that result in true/false.
 *
 * @param value Value to check
 * @return 1 if condition met, 0 otherwise
 */
```

## What NOT to Document

Avoid documenting:
1. Obvious standard library usage
2. Self-explanatory variable assignments
3. Standard design patterns
4. Temporary debug code (remove it instead)
5. TODO comments (use issue tracker instead, or mark with `TODO:` if temporary)

## Header Files (.h)

Document in headers when:
- Function is part of public API
- Struct members need explanation
- Macros are non-obvious

```c
/**
 * Brief description of structure purpose.
 */
typedef struct {
    int width;           // Width in pixels
    int height;          // Height in pixels
    void* pixels;        // Pixel data (caller must free)
} Surface;
```

## Updating Documentation

When modifying code:
1. **Update affected documentation** - Keep it in sync
2. **Add new examples** if behavior changes
3. **Document new edge cases** you discover
4. **Remove obsolete comments** - Outdated docs are worse than none

## Checklist for Code Reviews

- [ ] File header describes module purpose
- [ ] All public functions documented
- [ ] Complex private functions documented
- [ ] Non-obvious inline comments added
- [ ] Memory ownership clear
- [ ] Platform-specific code annotated
- [ ] Examples provided for complex transformations
- [ ] Edge cases documented
- [ ] No redundant or obvious comments

## Tools and Formatting

- Use `clang-format` to maintain consistent spacing
- Documentation comments use `/** */` style
- Inline comments use `//` style
- Keep line length under 100 characters (including comments)
- Align parameter descriptions for readability

---

**Remember:** Good documentation makes the code easier to understand, maintain, and extend. When in doubt, ask yourself: "Will I understand this code in 6 months?"
