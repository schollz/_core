# Prefer the download helper's location; retain the original local SDK layout.
RACK_DIR ?= $(if $(wildcard ../artifacts/rack-sdk/Rack-SDK/plugin.mk),../artifacts/rack-sdk/Rack-SDK,../artifacts/Rack-SDK)
CLANG ?= clang
PYTHON ?= python3
ENGINE_TARGET ?= $(CROSS_COMPILE)
SOURCES += $(wildcard src/*.cpp)
OBJECTS += build/core-engine.o build/card-format.o build/pcg.o
DISTRIBUTABLES += res README.md LICENSE.txt THIRD_PARTY.md licenses
EXTRA_CXXFLAGS += -std=c++17
EXTRA_FLAGS += -fwrapv -ffp-contract=off -fno-unsafe-math-optimizations
ifeq ($(shell uname -s),Darwin)
EXTRA_FLAGS += -mmacosx-version-min=11.0
endif

include $(RACK_DIR)/plugin.mk

build/generated/engine.c: $(wildcard ../lib/*.h) ../lib/crossfade4.py ../lib/fuzz.py ../lib/resonantfilter.py ../lib/core_engine/prepare.py $(wildcard ../lib/core_engine/*.h) $(wildcard ../lib/core_engine/*.inc) ../main.c
	$(PYTHON) ../lib/core_engine/prepare.py --out build/generated --clang $(CLANG) $(if $(ENGINE_TARGET),--target $(ENGINE_TARGET))

build/core-engine.o: build/generated/engine.c
	$(CC) $(CFLAGS) -std=c11 -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable -Wno-sign-compare -Wno-format -c $< -o $@

build/card-format.o: ../lib/core_engine/card_format.c ../lib/core_engine/card_format.h
	mkdir -p build
	$(CC) $(CFLAGS) -std=c11 -c $< -o $@

build/pcg.o: ../lib/pcg_basic.c
	mkdir -p build
	$(CC) $(CFLAGS) -std=c11 -c $< -o $@

test:
	$(PYTHON) ../test/rack/run.py

.PHONY: test install
