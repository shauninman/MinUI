# MinUI Technical Documentation

Welcome to the MinUI technical documentation. This documentation provides comprehensive information about MinUI's architecture, components, and development.

## Documentation Structure

### Getting Started
- [Architecture Overview](architecture.md) - High-level system design and philosophy
- [Project Structure](project-structure.md) - Directory layout and organization
- [Build System](build-system.md) - How to build MinUI from source

### Core Systems
- [Component Guide](components.md) - Detailed breakdown of all components
- [Platform Abstraction](platform-abstraction.md) - How platform support works
- [Pak System](pak-system.md) - Plugin architecture for emulators and tools

### Development
- [Development Guide](development-guide.md) - How to contribute and develop
- [Adding Platform Support](adding-platform.md) - Creating new platform ports
- [API Reference](api-reference.md) - Core APIs and functions

### Reference
- [Supported Platforms](platforms.md) - All supported devices and their details
- [File Formats](file-formats.md) - Configuration files and data structures
- [Troubleshooting](troubleshooting.md) - Common issues and solutions

## Quick Links

- [Main README](../README.md)
- [PAK Creation Guide](../PAKS.md)
- [User Guide](../skeleton/BASE/README.txt)

## About MinUI

MinUI is a minimal frontend launcher and libretro-based retro gaming environment designed for handheld Linux devices. It prioritizes:

- **Minimal resource usage** - Fast, lightweight C code
- **Direct hardware access** - No heavy frameworks or abstractions
- **Clean UI/UX** - Simple, intuitive navigation
- **Portability** - Support for 12+ device families
- **Extensibility** - Pak system for custom emulators and tools

## Key Statistics

- **Language**: C (C99 standard with GNU extensions)
- **Lines of Code**: ~30,000+ lines of C
- **Supported Platforms**: 12+ device families
- **Supported Consoles**: 8 stock + 6 additional emulators
- **Build System**: Docker-based cross-compilation
- **Dependencies**: SDL, libretro cores, minimal system libraries

## Development Status

MinUI is currently in a **deprecated maintenance mode** for the existing codebase. All current platforms are marked as deprecated, but the system remains functional and widely used.

This documentation aims to preserve knowledge about the system architecture and enable continued development and improvements.
