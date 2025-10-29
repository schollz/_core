# _core Repository - Copilot Coding Agent Instructions

## Repository Overview

This is the monorepo for **zeptocore**, **zeptoboard**, and **ectocore** music devices - open-source audio samplers and synthesizers built on the Raspberry Pi RP2040 microcontroller. The repository contains:

- **Firmware** (C/CMake for RP2040): Main firmware in `main.c` (~900 lines) with extensive DSP libraries in `lib/`
- **Core Tool** (Go): Sample loading web application in `core/` directory  
- **Documentation** (Hugo): Static site generator in `docs/` directory
- **Development utilities**: Python scripts for generating lookup tables, Go tools for audio processing

**Key technologies**: C11, ARM Cortex-M0+, CMake, Go 1.24+, Python 3.11+, Pico SDK 2.2.0, Hugo 0.135.0

**Repository size**: ~35,000 lines of C code in lib/, ~1,000 lines in main.c, substantial Go codebase in core/

## Critical Build Requirements

### Python Environment Setup (ALWAYS REQUIRED)

**ALWAYS** set up the Python virtual environment before any firmware-related operations:

```bash
# Install uv if not available (pip3 install uv)
uv venv .venv
source .venv/bin/activate  # On every new shell session
uv pip install -r requirements.txt
```

Python dependencies: numpy, scipy, matplotlib, pillow, tqdm, icecream (see requirements.txt)

### Firmware Build Prerequisites

**ALWAYS** install these dependencies before attempting firmware builds:

```bash
sudo apt-get update
sudo apt-get install -y cmake gcc-arm-none-eabi libnewlib-arm-none-eabi \
    build-essential gcc wget tar clang-format sox libsox-fmt-mp3
```

ARM compiler version: gcc-arm-none-eabi 13.2.1 or compatible

### Pico SDK Installation (REQUIRED for firmware builds)

The Pico SDK is **NOT** included in the repository. **ALWAYS** check if it exists before building:

```bash
# Check if SDK exists
ls pico-sdk pico-extras

# If missing, clone and initialize (this is part of the build process):
# SDK version MUST be 2.2.0
git clone https://github.com/raspberrypi/pico-sdk
cd pico-sdk && git checkout 2.2.0 && git submodule update --init --recursive && cd ..

git clone https://github.com/raspberrypi/pico-extras.git pico-extras
cd pico-extras && git checkout sdk-2.2.0 && git submodule update --init --recursive && cd ..

export PICO_SDK_PATH=$(pwd)/pico-sdk
export PICO_EXTRAS_PATH=$(pwd)/pico-extras
```

**CRITICAL PATCH**: After cloning pico-sdk, you **MUST** apply this patch (required by CI):

```bash
sed -i 's/OSAL_TIMEOUT_WAIT_FOREVER/OSAL_TIMEOUT_NORMAL/g' pico-sdk/lib/tinyusb/src/tusb.c
```

## Firmware Build Process

### Generated Header Files (ALWAYS generate before building)

Several header files are generated from Python scripts. **ALWAYS** generate these before building:

```bash
# Activate Python venv first!
source .venv/bin/activate

# Generate all required headers (takes ~10-20 seconds):
make lib/fuzz.h
make lib/transfer_saturate2.h
make lib/sinewaves2.h
make lib/crossfade4_441.h
make lib/resonantfilter_data.h
```

**Audio cuedsounds** (optional but recommended for full build):

```bash
# Build the audio2flash tool first
cd dev/audio2flash && go build -v && cd ../..

# Generate cuedsounds headers (requires sox)
make lib/cuedsounds.h  # Generates both zeptocore and ectocore variants
```

### Building Firmware Variants

The repository supports multiple firmware variants with different configurations:

**Common build commands** (each takes ~2-5 minutes on modern hardware):

```bash
# Clean build directory before switching variants
make clean

# Zeptocore variants (handheld device)
make zeptocore              # Normal overclock version
make zeptocore_128          # Ultra-low latency (128 samples)
make zeptocore_256          # Low latency (256 samples)
make zeptocore_nooverclock  # Stable clock version

# Ectocore variants (Eurorack module)
make ectocore               # Normal overclock version
make ectocore_128           # Ultra-low latency
make ectocore_256           # Low latency
make ectocore_noclock       # No overclock version
make ectocore_noclock_128   # No overclock + ultra-low latency
make ectocore_noclock_256   # No overclock + low latency

# Zeptoboard (breadboard development version)
make zeptoboard

# Development/testing build
make dobuild                # Quick build with default config
```

**Build output**: `.uf2` firmware files (e.g., `zeptocore.uf2`, `ectocore.uf2`)

### Build Configuration Files

Each variant uses a different compile definitions file:
- `zeptocore_compile_definitions.cmake` - Zeptocore configuration
- `ectocore_compile_definitions.cmake` - Ectocore configuration  
- `ectocore_compile_definitions_nooverclock.cmake` - No overclock version
- `zeptoboard_compile_definitions.cmake` - Breadboard version
- Multiple latency variants: `*_128.cmake` (128 samples), `*_256.cmake` (256 samples)

The Makefile copies the appropriate file to `target_compile_definitions.cmake` during build.

**Build artifacts location**: `build/` directory (gitignored)

## Core Tool (Go Application)

The core tool is a web-based sample loading application.

### Building the Core Tool

```bash
cd core
go build -v  # Takes ~20-30 seconds, produces 'core' binary

# Test the build
./core --help

# Build options:
# For ectocore mode: go build -v -ldflags "-X main.EctocoreDefault=yes"
# For zeptocore mode: go build -v -ldflags "-X main.ZeptocoreDefault=yes"
```

**Dependencies**: 
- Go 1.24+ required
- No CGO by default (pure Go build)
- Imports: gorilla/websocket, schollz/logger, go-audio/wav, etc.

**Running the tool**:
```bash
./core              # Starts server on :8101 (zeptocore) or :8100 (ectocore)
./core --dontopen   # Don't auto-open browser
./core --ectocore   # Force ectocore mode
./core --log trace  # Verbose logging
```

The tool serves a web interface for loading samples onto connected devices via USB.

### Core Tool Development

```bash
# Install air for hot-reload development
go install github.com/cosmtrek/air@latest

# Run development server with auto-reload
cd core && air
# Or: make server
```

## Documentation Build

The documentation uses Hugo static site generator.

### Building Documentation

```bash
# Install Hugo 0.135.0 (exact version used in CI)
# Download from: https://github.com/gohugoio/hugo/releases/tag/v0.135.0

cd docs
hugo --minify  # Takes ~1 second, outputs to docs/public/

# Development server with live reload:
hugo serve -D --bind 0.0.0.0
# Or: make docs (from repo root)
```

**Documentation structure**:
- `docs/content/` - Markdown content files
- `docs/static/` - Static assets (images, audio files, JSON data)
- `docs/themes/zeptocore/` - Custom Hugo theme
- `docs/hugo.toml` - Hugo configuration

**Generated docs location**: `docs/public/` (gitignored, embedded in Go tool)

## Common Build Errors and Solutions

### "pico-sdk not found" or "PICO_SDK_PATH not set"

**Solution**: Clone and checkout pico-sdk version 2.2.0, then set environment variable:
```bash
git clone https://github.com/raspberrypi/pico-sdk
cd pico-sdk && git checkout 2.2.0 && git submodule update --init --recursive && cd ..
export PICO_SDK_PATH=$(pwd)/pico-sdk
```

### "lib/fuzz.h: No such file or directory"

**Solution**: Generate header files first (requires Python venv):
```bash
source .venv/bin/activate
make lib/fuzz.h lib/transfer_saturate2.h lib/sinewaves2.h lib/crossfade4_441.h lib/resonantfilter_data.h
```

### "arm-none-eabi-gcc: command not found"

**Solution**: Install ARM toolchain:
```bash
sudo apt-get install gcc-arm-none-eabi libnewlib-arm-none-eabi
```

### "sox: command not found" (when building cuedsounds)

**Solution**: Install sox:
```bash
sudo apt-get install sox libsox-fmt-mp3
```

### Build fails with "target_compile_definitions.cmake not found"

**Solution**: Use a make target (not cmake directly) - the Makefile creates this file:
```bash
make zeptocore  # Not: cmake .. && make
```

### Python import errors (numpy, scipy, etc.)

**Solution**: Ensure virtual environment is activated and dependencies installed:
```bash
source .venv/bin/activate
uv pip install -r requirements.txt
```

## Repository Structure

### Root Directory Files
- `main.c` - Main firmware entry point (~900 lines)
- `CMakeLists.txt` - Top-level CMake configuration
- `Makefile` - Primary build orchestration (handles all variants)
- `requirements.txt` - Python dependencies for code generation
- `tusb_config.h` - TinyUSB configuration
- `pico_sdk_import.cmake`, `pico_extras_import.cmake` - SDK imports
- Multiple `*_compile_definitions.cmake` - Variant-specific configurations

### Key Directories

**`lib/`** - DSP libraries and firmware components (~35,000 lines of C)
- Header-only DSP libraries: `beatrepeat.h`, `bitcrush.h`, `comb.h`, `delay.h`, `freeverb_fp.h`, `fuzz.py`, `resonantfilter.h`, `saturation.h`, `tapedelay.h`, etc.
- Hardware abstraction: `sdcard.h`, `ssd1306.h`, `buttonmatrix3.h`, `WS2812.h`
- Audio processing: `audio_callback.h`, `wavetablesyn.h`, `sequencer.h`
- Device configs: `ectocore.h`, `zeptocore.h`, `zeptoboard.h`
- Generated headers (not in repo): `fuzz.h`, `transfer_saturate2.h`, `sinewaves2.h`, `crossfade4_*.h`, `resonantfilter_data.h`, `cuedsounds_*.h`
- Python generators: `fuzz.py`, `crossfade4.py`, `resonantfilter.py`, `biquad.py`, etc.

**`core/`** - Go sample loading tool application
- `main.go` - Application entry point
- `go.mod`, `go.sum` - Go dependencies
- `src/` - Source packages (server, sox wrapper, minicom, utilities)
- `.air.toml` - Air hot-reload configuration
- `.goreleaser.yaml` - Release build configuration

**`docs/`** - Hugo documentation site
- `content/` - Markdown documentation (blurbs, combos, effects, questions, quickstart)
- `static/` - Static assets (images, audio samples, JavaScript, CSS)
- `themes/zeptocore/` - Custom theme
- `hugo.toml` - Site configuration

**`dev/`** - Development utilities and tools
- `audio2flash/` - Go tool to convert audio files to header files
- Various testing utilities and scripts

**`test/`** - Test code and examples

**`schematics/`** - Hardware schematics (PDFs)

**`.github/workflows/`** - CI/CD pipelines
- `build.yml` - Firmware build and release workflow
- `build-core.yml` - Core tool build for multiple platforms

## CI/CD Validation

The GitHub Actions workflows provide the authoritative build process:

### Firmware Build Workflow (`build.yml`)

**Runs on**: ubuntu-24.04
**Trigger**: Push to main, tags (v* or *.*.*), PRs, manual dispatch

**Build sequence**:
1. Install: sox, libsox-fmt-mp3, Hugo 0.135.0, Python 3.11, Zig 0.11.0, cmake, gcc-arm-none-eabi
2. Create Python venv with uv, install requirements
3. Clone pico-sdk (2.2.0) and pico-extras (sdk-2.2.0) with submodules
4. **CRITICAL**: Patch tinyusb (sed replace OSAL_TIMEOUT_WAIT_FOREVER)
5. Build all firmware variants using Makefile targets
6. On tags: Create source tarball with vendored Go modules
7. Upload artifacts and create GitHub release

**Builds produced**: zeptocore, zeptocore_128, zeptocore_256, zeptoboard, ectocore, ectocore_128, ectocore_256, ectocore_noclock, ectocore_noclock_128, ectocore_noclock_256, ectocore_beta_hardware

### Core Tool Build Workflow (`build-core.yml`)

**Platforms**: macOS (latest), Ubuntu (latest), Windows (latest)
**Trigger**: Push to main, tags, PRs

**Build sequence**:
- macOS: Install rtmidi, pkg-config, sox via brew; build with CGO_ENABLED=1
- Linux: Install libasound2-dev, sox; build with standard Go
- Windows: Use MSYS2 with mingw64, rtmidi; build with mingw32-gcc

**Outputs**: Zipped binaries for each platform (ectocore_tool_*, zeptocore_tool_*)

## Working with the Codebase

### Making Firmware Changes

1. **Always activate Python venv**: `source .venv/bin/activate`
2. If modifying DSP or adding effects, check if new Python generators needed
3. Regenerate headers after Python script changes: `make lib/[filename].h`
4. Use `make clean` when switching between variants
5. Test with multiple variants if changes affect core functionality
6. Check `main.c` for conditional compilation (`#ifdef INCLUDE_ECTOCORE`, etc.)

### Hardware-Specific Code

Look for these preprocessor flags in the code:
- `INCLUDE_ECTOCORE` - Ectocore-specific features
- `INCLUDE_ZEPTOCORE` - Zeptocore-specific features
- `INCLUDE_ZEPTOBOARD` - Zeptoboard-specific features
- `DO_OVERCLOCK` - Overclocking configuration
- `INCLUDE_MIDI`, `INCLUDE_FILTER`, `INCLUDE_RGBLED` - Optional features
- `PRINT_AUDIO_USAGE`, `PRINT_MEMORY_USAGE` - Debug output flags

### Code Formatting

C code uses Google style via clang-format:
```bash
clang-format -i --style=google lib/yourfile.h
```

Generated headers are automatically formatted by the Makefile.

### Testing Changes

No automated test suite exists. Validation is done by:
1. Building firmware for target variant(s)
2. Flashing to physical hardware via BOOTSEL mode (copy .uf2 to RPI-RP2 drive)
3. Manual testing with audio samples and hardware controls

For core tool changes:
1. Build and run locally: `cd core && go build && ./core`
2. Test web interface in browser (auto-opens to localhost:8100 or :8101)
3. Test with actual hardware if USB communication involved

## Important Notes

### Build Times
- Firmware build (single variant): ~2-5 minutes
- Core tool build: ~20-30 seconds  
- Documentation build: ~1 second
- Full CI pipeline (all variants): ~15-20 minutes

### Memory Constraints
The RP2040 has limited RAM (264KB). The code uses:
- Fixed-point math (not floating point)
- Careful memory management in DSP libraries
- Flash storage for lookup tables (`__in_flash()` attribute)
- Different sample buffer sizes for latency variants (128/256/441 samples)

### SDK Version Compatibility
**CRITICAL**: Pico SDK version must be exactly 2.2.0, pico-extras must be sdk-2.2.0. Other versions may not work due to API changes or the required tinyusb patch.

### Build System Hierarchy
- **Makefile** is the primary interface (handles variant selection, header generation, SDK setup)
- **CMakeLists.txt** is called by Makefile (handles actual compilation)
- **Never run cmake directly** - always use Makefile targets

## Quick Reference Commands

```bash
# Setup (once per environment)
uv venv .venv && source .venv/bin/activate
uv pip install -r requirements.txt
sudo apt-get install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi sox clang-format

# Build firmware (choose one variant)
make zeptocore      # or ectocore, zeptoboard, etc.

# Build core tool
cd core && go build -v

# Build documentation  
cd docs && hugo --minify

# Development
cd core && air                # Core tool with hot-reload
cd docs && hugo serve -D      # Docs with live preview

# Clean up
make clean                    # Remove build/ directory
```

## Summary for Agents

When working on this repository:

1. **ALWAYS activate Python venv** before firmware operations
2. **ALWAYS generate header files** before building firmware
3. **ALWAYS use Makefile targets** (not cmake directly) for firmware builds
4. **ALWAYS ensure pico-sdk 2.2.0** is present with tinyusb patch applied
5. **Verify dependencies** installed before attempting builds
6. **Use appropriate variant** - don't assume "zeptocore" if working on ectocore
7. **Check CI workflows** for authoritative build process if uncertain
8. **Don't commit generated files** - fuzz.h, crossfade4_*.h, cuedsounds_*.h, build/, etc. are gitignored

Build failures are almost always due to: missing pico-sdk, missing generated headers, missing Python venv activation, or missing system dependencies. Check these first.
