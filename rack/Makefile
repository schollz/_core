PYTHON ?= python3
RELEASE_ARGS ?=
MACOS_RELEASE_HOST ?= zns@192.168.0.44
RELEASE_GOALS := release-macosarm release-macos11 release-linux

# Release commands fetch their own SDK inside a fresh clone. Ordinary builds
# retain the SDK's default goal and existing local SDK selection.
ifneq ($(strip $(filter-out $(RELEASE_GOALS),$(or $(MAKECMDGOALS),all))),)
# Prefer the download helper's location; retain the original local SDK layout.
RACK_DIR ?= $(if $(wildcard ../artifacts/rack-sdk/Rack-SDK/plugin.mk),../artifacts/rack-sdk/Rack-SDK,../artifacts/Rack-SDK)
SOURCES += $(wildcard src/*.cpp)
OBJECTS += build/core-engine.o build/card-format.o build/pcg.o
DISTRIBUTABLES += res README.md LICENSE.txt THIRD_PARTY.md licenses
EXTRA_CXXFLAGS += -std=c++17
EXTRA_FLAGS += -fwrapv -ffp-contract=off -fno-unsafe-math-optimizations

include $(RACK_DIR)/plugin.mk

# VCV exports an osxcross Clang wrapper as CC without CROSS_COMPILE. Use it
# for both generation and compilation so target libc headers and CPU agree.
# Linux/Windows may use GCC for CC, but the generated unit requires Clang.
CLANG ?= $(if $(ARCH_MAC),$(CC),clang)

# VCV's Windows cross toolchain exports CC but not CROSS_COMPILE. MACHINE is
# resolved by the SDK from CC -dumpmachine. Both preprocessing and compilation
# must select MinGW, including its libc headers rather than the build host's.
ENGINE_TARGET ?= $(or $(CROSS_COMPILE),$(if $(ARCH_WIN),$(MACHINE)))
# crosstool-ng's GCC sysroot contains mingw -> usr/<target>, rather than
# include/ at its root. Clang's MinGW driver needs that inner runtime root.
# Native MSYS2 GCC reports no sysroot; leave Clang's discovery intact there.
ENGINE_SYSROOT ?= $(if $(ARCH_WIN),$(shell \
	gcc_sysroot="$$($(CC) -print-sysroot)"; \
	if test -n "$$gcc_sysroot" && test -f "$$gcc_sysroot/mingw/include/_mingw.h"; then \
		printf '%s' "$$gcc_sysroot/mingw"; \
	else \
		printf '%s' "$$gcc_sysroot"; \
	fi))
ENGINE_CLANG_FLAGS = $(if $(ENGINE_TARGET),--target=$(ENGINE_TARGET)) $(if $(ENGINE_SYSROOT),--sysroot="$(ENGINE_SYSROOT)")

# std::filesystem requires Catalina on Intel; Apple Silicon starts at Big Sur.
# Use the SDK's target architecture, including when VCV builds on a Linux host.
# EXTRA_FLAGS is expanded in compile/link recipes after the SDK's 10.9 default.
ifdef ARCH_MAC
ifdef ARCH_ARM64
EXTRA_FLAGS += -mmacosx-version-min=11.0
else
EXTRA_FLAGS += -mmacosx-version-min=10.15
endif
endif

# Order-only checks also cover direct binary targets without forcing relinks.
.PHONY: check-manifest
check-manifest:
	$(PYTHON) scripts/manifest.py --check

all $(TARGET) $(OBJECTS): | check-manifest

build/generated/engine.c: $(wildcard ../lib/*.h) ../lib/crossfade4.py ../lib/fuzz.py ../lib/resonantfilter.py ../lib/core_engine/prepare.py $(wildcard ../lib/core_engine/*.h) $(wildcard ../lib/core_engine/*.inc) ../main.c
	$(PYTHON) ../lib/core_engine/prepare.py --out build/generated --clang $(CLANG) $(if $(ENGINE_TARGET),--target $(ENGINE_TARGET)) $(if $(ENGINE_SYSROOT),--sysroot "$(ENGINE_SYSROOT)")

build/core-engine.o: build/generated/engine.c
	# The generated unit contains Clang's preprocessed libc and atomic builtins.
	# -fno-gnu-unique is a GCC-only flag added by the Linux Rack SDK.
	$(CLANG) $(filter-out -fno-gnu-unique,$(CFLAGS)) $(ENGINE_CLANG_FLAGS) -std=c11 -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable -Wno-sign-compare -Wno-format -c $< -o $@

build/card-format.o: ../lib/core_engine/card_format.c ../lib/core_engine/card_format.h
	mkdir -p build
	$(CC) $(CFLAGS) -std=c11 -c $< -o $@

build/pcg.o: ../lib/pcg_basic.c
	mkdir -p build
	$(CC) $(CFLAGS) -std=c11 -c $< -o $@

test:
	$(PYTHON) ../test/rack/run.py

# Keep the SDK's install recipe, including its native packaging and user path.
install-vcv: install

.PHONY: test install install-vcv
endif

.PHONY: $(RELEASE_GOALS)
release-macosarm:
	$(PYTHON) Release/release-macosarm.py $(RELEASE_ARGS)
release-macos11:
	$(PYTHON) Release/release-macos11.py "$(MACOS_RELEASE_HOST)" $(RELEASE_ARGS)
release-linux:
	$(PYTHON) Release/release-linux.py $(RELEASE_ARGS)
