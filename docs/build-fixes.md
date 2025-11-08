# MinUI Build Fixes

This document describes fixes applied to the MinUI build system to address common issues.

## Automatic Dockerfile Patching

**Problem:** The external toolchain repositories use Debian Buster, which was archived in 2022 and is no longer available from standard Debian mirrors.

**Error:**
```
Err:4 http://deb.debian.org/debian buster Release
  404  Not Found
E: The repository 'http://deb.debian.org/debian buster Release' does not have a Release file.
```

**Solution:** The `makefile.toolchain` now automatically patches Dockerfiles when cloning toolchain repositories.

### What Gets Patched

When a toolchain is cloned, the following changes are automatically applied to the Dockerfile:

1. **Keeps Debian Buster** - No version change to avoid compatibility issues
2. **Redirects apt to archive.debian.org** - Points to where Buster packages are still available
3. **Adds all three repos**:
   - `deb http://archive.debian.org/debian buster main`
   - `deb http://archive.debian.org/debian-security buster/updates main`
   - `deb http://archive.debian.org/debian buster-updates main`
4. **Disables apt valid-until check** - Archived repos don't get updates, so this prevents errors

### Implementation

The patching happens in `/makefile.toolchain`:

```makefile
$(GIT_IF_NECESSARY):
	mkdir -p toolchains
	git clone https://github.com/shauninman/union-$(PLATFORM)-toolchain/ toolchains/$(PLATFORM)-toolchain
	@echo "Patching Dockerfile for archived Debian versions..."
	@if [ -f toolchains/$(PLATFORM)-toolchain/Dockerfile ]; then \
		DOCKERFILE=toolchains/$(PLATFORM)-toolchain/Dockerfile; \
		if grep -q "^FROM debian:buster" $$DOCKERFILE; then \
			awk '/^FROM debian:buster/ {print; print "RUN echo \"deb http://archive.debian.org/debian buster main\" > /etc/apt/sources.list && \\"; print "    echo \"deb http://archive.debian.org/debian-security buster/updates main\" >> /etc/apt/sources.list && \\"; print "    echo \"deb http://archive.debian.org/debian buster-updates main\" >> /etc/apt/sources.list && \\"; print "    echo \"Acquire::Check-Valid-Until false;\" > /etc/apt/apt.conf.d/99no-check-valid-until"; next}1' $$DOCKERFILE > $$DOCKERFILE.tmp && \
			mv $$DOCKERFILE.tmp $$DOCKERFILE; \
			echo "✓ Dockerfile patched successfully"; \
		else \
			echo "✓ No Debian Buster found, skipping patch"; \
		fi; \
	fi
```

### How It Works

The script uses `awk` to:
1. Find the `FROM debian:buster*` line
2. Insert a new `RUN` command immediately after it that:
   - **Overwrites** `/etc/apt/sources.list` with archive.debian.org URLs
   - Adds all three Debian repos (main, security, updates)
   - Creates apt config to disable valid-until checking

**Why awk instead of sed?**
- Works identically on macOS (BSD) and Linux (GNU)
- No need for platform-specific syntax
- Cleaner multi-line insertion

**Why overwrite sources.list?**
- Old URLs (`deb.debian.org`) return 404 errors
- Clean replacement = no failed connection attempts
- Matches original repo structure (main, security, updates)

### Verification

After cloning a toolchain, you can verify the patch was applied:

```bash
head -10 toolchains/rg35xx-toolchain/Dockerfile
```

Should show:
```dockerfile
FROM debian:buster-slim
RUN echo "deb http://archive.debian.org/debian buster main" > /etc/apt/sources.list && \
    echo "deb http://archive.debian.org/debian-security buster/updates main" >> /etc/apt/sources.list && \
    echo "deb http://archive.debian.org/debian buster-updates main" >> /etc/apt/sources.list && \
    echo "Acquire::Check-Valid-Until false;" > /etc/apt/apt.conf.d/99no-check-valid-until
ENV DEBIAN_FRONTEND noninteractive
```

### Future-Proofing

When Debian Bullseye eventually gets archived, this approach can be updated to patch to a newer version (e.g., Debian Bookworm) without modifying the external toolchain repositories.

To update to a newer Debian version in the future, simply modify the sed patterns in `makefile.toolchain`.

## Why This Approach?

Instead of forking all 12+ toolchain repositories, this approach:

1. **Avoids maintenance burden** - No need to maintain forked repos
2. **Stays up-to-date** - Gets updates from upstream repos automatically
3. **Transparent** - Clearly shows what's being modified
4. **Future-proof** - Easy to update for future Debian versions
5. **Zero friction** - Users don't need to do anything special

## Resetting the Build Environment

### Quick Clean (Build Artifacts Only)
```bash
make clean
```
Removes `./build/` directory but keeps toolchains and Docker images.

### Full Reset (Start Fresh)
```bash
# Remove build artifacts
make clean

# Remove downloaded toolchains
rm -rf toolchains/

# Remove releases (optional)
rm -rf releases/
```

### Per-Platform Reset
```bash
# Remove specific toolchain and rebuild
rm -rf toolchains/miyoomini-toolchain
make PLATFORM=miyoomini

# Or clean Docker image only (keeps toolchain source)
make -f makefile.toolchain PLATFORM=miyoomini clean
```

### Nuclear Option (Complete Wipe)
```bash
# Clean everything
make clean
rm -rf toolchains/ releases/ build/

# Also remove Docker images
docker images | grep toolchain | awk '{print $3}' | xargs docker rmi
```

## Troubleshooting

### Patch didn't apply

If the Dockerfile wasn't patched automatically:

```bash
# Remove and re-clone
rm -rf toolchains/miyoomini-toolchain
make -f makefile.toolchain PLATFORM=miyoomini toolchains/miyoomini-toolchain
```

### Manual patching

If you need to manually patch a Dockerfile:

```bash
cd toolchains/miyoomini-toolchain
sed -i 's/debian:buster/debian:bullseye/g' Dockerfile  # Linux
# or
sed -i '' 's/debian:buster/debian:bullseye/g' Dockerfile  # macOS
```

### Still getting 404 errors

If you still see 404 errors after patching:

1. Check if Docker has cached the old Buster image:
   ```bash
   docker images | grep buster
   docker rmi debian:buster-slim  # Remove if present
   ```

2. Force rebuild:
   ```bash
   cd toolchains/miyoomini-toolchain
   make clean
   make .build
   ```

## Obsolete PokeMini Patch Removed

**Problem:** The pokemini core build was failing with:
```
error: patch failed: source/PokeMini.c:489
error: source/PokeMini.c: patch does not apply
```

**Root Cause:** The patch `workspace/all/cores/patches/pokemini/0001-fix-resume-audio.patch` was attempting to fix a bug where `"LCD-"` should have been `"AUD-"` for audio state loading. However, upstream pokemini has since fixed this bug, making the patch obsolete and causing conflicts.

**Solution:** Removed the obsolete patch directory `workspace/all/cores/patches/pokemini/` since the fix is now included in the upstream libretro pokemini core.

**Verification:**
```bash
# Check that upstream has the fix
grep "AUD-" workspace/miyoomini/cores/src/pokemini/source/PokeMini.c
# Should show line 407: } else if (!strcmp(PMiniStr, "AUD-")) {
```

## Related Issues

This document addresses:
- Debian Buster 404 errors (archived ~June 2022)
- PokeMini patch conflicts (fixed upstream)
- Affected platforms: All (miyoomini, rg35xx, trimuismart, etc.)

## Contributing

If you encounter other build system issues, please:
1. Document the problem
2. Propose a fix
3. Test on both macOS and Linux (if possible)
4. Add to this document
