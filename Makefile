# Makefile
export PICO_EXTRAS_PATH=$(CURDIR)/pico-extras
PICO_SDK_VERSION := 2.3.1
PICO_EXTRAS_VERSION := sdk-2.3.1
export PICO_SDK_PATH=$(CURDIR)/pico-sdk
UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)
ifeq ($(UNAME_S),Darwin)
NPROCS := $(shell sysctl -n hw.logicalcpu 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 1)
else
NPROCS := $(shell nproc 2>/dev/null || getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)
endif
MACOS_ARM_TC_VERSION ?= 14.2.1-1.1
ifeq ($(UNAME_M),arm64)
MACOS_ARM_TC_ARCH := arm64
else
MACOS_ARM_TC_ARCH := x64
endif
MACOS_ARM_TC_BASENAME := xpack-arm-none-eabi-gcc-$(MACOS_ARM_TC_VERSION)-darwin-$(MACOS_ARM_TC_ARCH)
MACOS_ARM_TC_DIR := $(HOME)/.cache/_core/toolchains/xpack-arm-none-eabi-gcc-$(MACOS_ARM_TC_VERSION)
MACOS_ARM_TC_BIN := $(MACOS_ARM_TC_DIR)/bin
MACOS_ARM_TC_URL := https://github.com/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases/download/v$(MACOS_ARM_TC_VERSION)/$(MACOS_ARM_TC_BASENAME).tar.gz
# Always pass an explicit value: a previous ON CMake cache must not opt in later builds.
ZEPTOCORE_VISUALIZER ?= OFF
ifeq ($(filter ON OFF,$(ZEPTOCORE_VISUALIZER)),)
$(error ZEPTOCORE_VISUALIZER must be ON or OFF)
endif
VISUALIZER_SUFFIX := $(if $(filter ON,$(ZEPTOCORE_VISUALIZER)),_visualizer,)
CORE_COMPILE_DEFINITIONS ?= $(CURDIR)/lib/cmake/target_compile_definitions.cmake
CORE_NO_OVERCLOCK ?= OFF
CORE_CMAKE_OPTIONS = -DZEPTOCORE_VISUALIZER=$(ZEPTOCORE_VISUALIZER) -DCORE_COMPILE_DEFINITIONS="$(CORE_COMPILE_DEFINITIONS)" -DCORE_NO_OVERCLOCK=$(CORE_NO_OVERCLOCK)
PICOTOOL ?= picotool
UPLOAD_UF2 ?= build/_core.uf2
PICOTOOL_LOAD_FLAGS ?= --family rp2040 -f -x -v
UPLOAD_TOUCH_1200 ?= 1
UPLOAD_TOUCH_BAUD ?= 1200
UPLOAD_TOUCH_WAIT ?= 2
UPLOAD_MIDI_RESET ?= 1
GOVERSION = go1.21.13
GOBIN = $(HOME)/go/bin
GOINSTALLPATH = $(GOBIN)/$(GOVERSION)

dobuild: pico-sdk pico-extras lib/fuzz.h lib/transfer_saturate2.h lib/sinewaves2.h lib/crossfade4_441.h lib/resonantfilter_data.h lib/cuedsounds.h build
	make -C build -j$(NPROCS)

pico-sdk:
	git clone --branch $(PICO_SDK_VERSION) --depth 1 --recurse-submodules --shallow-submodules https://github.com/raspberrypi/pico-sdk pico-sdk

chowdsp:
	sudo apt install -y lv2file
	wget https://github.com/jatinchowdhury18/AnalogTapeModel/releases/download/v2.11.4/ChowTapeModel-Linux-x64-2.11.4.deb
	sudo dpkg --install ChowTapeModel-Linux-x64-2.11.4.deb

install_go21:
	@if [ -x "$(GOINSTALLPATH)" ]; then \
		echo "$(GOVERSION) is already installed."; \
		exit 0; \
	else \
		echo "$(GOVERSION) is not installed."; \
		go install golang.org/dl/$(GOVERSION)@latest; \
		$(GOINSTALLPATH) download; \
	fi

zeptoboard: pico-sdk pico-extras copyboard lib/fuzz.h lib/transfer_saturate2.h lib/sinewaves2.h lib/crossfade4_441.h lib/resonantfilter_data.h lib/cuedsounds.h build
	make -C build -j$(NPROCS)
	cp build/_core.uf2 zeptoboard.uf2

# Select the device before configuration, including parallel Make invocations.
zeptocore zeptocore_nooverclock: CORE_COMPILE_DEFINITIONS = $(CURDIR)/lib/cmake/zeptocore_compile_definitions.cmake
zeptocore_128: CORE_COMPILE_DEFINITIONS = $(CURDIR)/lib/cmake/zeptocore_compile_definitions_128.cmake
zeptocore_256: CORE_COMPILE_DEFINITIONS = $(CURDIR)/lib/cmake/zeptocore_compile_definitions_256.cmake
zeptocore_nooverclock: CORE_NO_OVERCLOCK = ON
zeptoboard: CORE_COMPILE_DEFINITIONS = $(CURDIR)/lib/cmake/zeptoboard_compile_definitions.cmake
ectocore: CORE_COMPILE_DEFINITIONS = $(CURDIR)/lib/cmake/ectocore_compile_definitions.cmake
ectocore_128: CORE_COMPILE_DEFINITIONS = $(CURDIR)/lib/cmake/ectocore_compile_definitions_128.cmake
ectocore_64: CORE_COMPILE_DEFINITIONS = $(CURDIR)/lib/cmake/ectocore_compile_definitions_64.cmake
ectocore_256: CORE_COMPILE_DEFINITIONS = $(CURDIR)/lib/cmake/ectocore_compile_definitions_256.cmake
ectocore_beta_hardware: CORE_COMPILE_DEFINITIONS = $(CURDIR)/lib/cmake/ectocore_compile_definitions_v0.3.cmake
ectocore_noclock: CORE_COMPILE_DEFINITIONS = $(CURDIR)/lib/cmake/ectocore_compile_definitions_nooverclock.cmake
ectocore_noclock_128: CORE_COMPILE_DEFINITIONS = $(CURDIR)/lib/cmake/ectocore_compile_definitions_nooverclock_128.cmake
ectocore_noclock_256: CORE_COMPILE_DEFINITIONS = $(CURDIR)/lib/cmake/ectocore_compile_definitions_nooverclock_256.cmake
ezeptocore_midi: CORE_COMPILE_DEFINITIONS = $(CURDIR)/lib/cmake/ezeptocore_midi_compile_definitions.cmake
ezeptocore: CORE_COMPILE_DEFINITIONS = $(CURDIR)/lib/cmake/ezeptocore_compile_definitions.cmake
ezeptocore_128: CORE_COMPILE_DEFINITIONS = $(CURDIR)/lib/cmake/ezeptocore_compile_definitions_128.cmake
ezeptocore_256: CORE_COMPILE_DEFINITIONS = $(CURDIR)/lib/cmake/ezeptocore_compile_definitions_256.cmake
ezeptocore_noclock: CORE_COMPILE_DEFINITIONS = $(CURDIR)/lib/cmake/ezeptocore_compile_definitions_nooverclock.cmake
ezeptocore_noclock_128: CORE_COMPILE_DEFINITIONS = $(CURDIR)/lib/cmake/ezeptocore_compile_definitions_nooverclock_128.cmake
ezeptocore_noclock_256: CORE_COMPILE_DEFINITIONS = $(CURDIR)/lib/cmake/ezeptocore_compile_definitions_nooverclock_256.cmake

# Sequential submakes keep upload after the successful build, even with make -j.
.PHONY: zeptocore-visualizer
zeptocore-visualizer:
	$(MAKE) zeptocore ZEPTOCORE_VISUALIZER=ON
	$(MAKE) upload-built UPLOAD_UF2=zeptocore_visualizer.uf2

.PHONY: zeptocore zeptocore_128 zeptocore_256 zeptocore_nooverclock
zeptocore zeptocore_128 zeptocore_256: pico-sdk pico-extras lib/fuzz.h lib/transfer_saturate2.h lib/sinewaves2.h lib/crossfade4_441.h lib/resonantfilter_data.h lib/cuedsounds.h build
	cp "$(CORE_COMPILE_DEFINITIONS)" lib/cmake/target_compile_definitions.cmake
	cp build/_core.uf2 zeptocore$(VISUALIZER_SUFFIX).uf2

zeptocore_nooverclock: pico-sdk pico-extras lib/fuzz.h lib/transfer_saturate2.h lib/sinewaves2.h lib/crossfade4_441.h lib/resonantfilter_data.h lib/cuedsounds.h build
	cp "$(CORE_COMPILE_DEFINITIONS)" lib/cmake/target_compile_definitions.cmake
	sed -i 's/DO_OVERCLOCK=1/#DO_OVERCLOCK=1/g' lib/cmake/target_compile_definitions.cmake
	cp build/_core.uf2 zeptocore_nooverclock$(VISUALIZER_SUFFIX).uf2

ectocore: pico-sdk pico-extras copyecto lib/fuzz.h lib/transfer_saturate2.h lib/sinewaves2.h lib/crossfade4_441.h lib/resonantfilter_data.h lib/cuedsounds.h build
	make -C build -j$(NPROCS)
	cp build/_core.uf2 ectocore.uf2

ectocore_128: pico-sdk pico-extras lib/fuzz.h lib/transfer_saturate2.h lib/sinewaves2.h lib/crossfade4_441.h lib/resonantfilter_data.h lib/cuedsounds.h build
	cp lib/cmake/ectocore_compile_definitions_128.cmake lib/cmake/target_compile_definitions.cmake
	make -C build -j$(NPROCS)
	cp build/_core.uf2 ectocore.uf2

ectocore_64: pico-sdk pico-extras lib/fuzz.h lib/transfer_saturate2.h lib/sinewaves2.h lib/crossfade4_441.h lib/resonantfilter_data.h lib/cuedsounds.h build
	cp lib/cmake/ectocore_compile_definitions_64.cmake lib/cmake/target_compile_definitions.cmake
	make -C build -j$(NPROCS)
	cp build/_core.uf2 ectocore.uf2

ectocore_256: pico-sdk pico-extras lib/fuzz.h lib/transfer_saturate2.h lib/sinewaves2.h lib/crossfade4_441.h lib/resonantfilter_data.h lib/cuedsounds.h build
	cp lib/cmake/ectocore_compile_definitions_256.cmake lib/cmake/target_compile_definitions.cmake
	make -C build -j$(NPROCS)
	cp build/_core.uf2 ectocore.uf2

ectocore_beta_hardware: pico-sdk pico-extras lib/fuzz.h lib/transfer_saturate2.h lib/sinewaves2.h lib/crossfade4_441.h lib/resonantfilter_data.h lib/cuedsounds.h build
	cp lib/cmake/ectocore_compile_definitions_v0.3.cmake lib/cmake/target_compile_definitions.cmake
	make -C build -j$(NPROCS)
	cp build/_core.uf2 ectocore_beta_hardware.uf2

ectocore_noclock: pico-sdk pico-extras copyectonoclock lib/fuzz.h lib/transfer_saturate2.h lib/sinewaves2.h lib/crossfade4_441.h lib/resonantfilter_data.h lib/cuedsounds.h build
	make -C build -j$(NPROCS)
	cp build/_core.uf2 ectocore.uf2

ectocore_noclock_128: pico-sdk pico-extras lib/fuzz.h lib/transfer_saturate2.h lib/sinewaves2.h lib/crossfade4_441.h lib/resonantfilter_data.h lib/cuedsounds.h build
	cp lib/cmake/ectocore_compile_definitions_nooverclock_128.cmake lib/cmake/target_compile_definitions.cmake
	make -C build -j$(NPROCS)
	cp build/_core.uf2 ectocore.uf2

ectocore_noclock_256: pico-sdk pico-extras lib/fuzz.h lib/transfer_saturate2.h lib/sinewaves2.h lib/crossfade4_441.h lib/resonantfilter_data.h lib/cuedsounds.h build
	cp lib/cmake/ectocore_compile_definitions_nooverclock_256.cmake lib/cmake/target_compile_definitions.cmake
	make -C build -j$(NPROCS)
	cp build/_core.uf2 ectocore.uf2

ezeptocore_midi: pico-sdk pico-extras ensure_arm_toolchain lib/fuzz.h lib/transfer_saturate2.h lib/sinewaves2.h lib/crossfade4_441.h lib/resonantfilter_data.h lib/cuedsounds.h build/Makefile
	cp lib/cmake/ezeptocore_midi_compile_definitions.cmake lib/cmake/target_compile_definitions.cmake
	$(MAKE) -C build -j$(NPROCS)
	cp build/_core.uf2 ezeptocore_midi.uf2

ezeptocore: pico-sdk pico-extras lib/fuzz.h lib/transfer_saturate2.h lib/sinewaves2.h lib/crossfade4_441.h lib/resonantfilter_data.h lib/cuedsounds.h build
	cp lib/cmake/ezeptocore_compile_definitions.cmake lib/cmake/target_compile_definitions.cmake
	make -C build -j$(NPROCS)
	cp build/_core.uf2 ezeptocore.uf2

ezeptocore_128: pico-sdk pico-extras lib/fuzz.h lib/transfer_saturate2.h lib/sinewaves2.h lib/crossfade4_441.h lib/resonantfilter_data.h lib/cuedsounds.h build
	cp lib/cmake/ezeptocore_compile_definitions_128.cmake lib/cmake/target_compile_definitions.cmake
	make -C build -j$(NPROCS)
	cp build/_core.uf2 ezeptocore.uf2

ezeptocore_256: pico-sdk pico-extras lib/fuzz.h lib/transfer_saturate2.h lib/sinewaves2.h lib/crossfade4_441.h lib/resonantfilter_data.h lib/cuedsounds.h build
	cp lib/cmake/ezeptocore_compile_definitions_256.cmake lib/cmake/target_compile_definitions.cmake
	make -C build -j$(NPROCS)
	cp build/_core.uf2 ezeptocore.uf2

ezeptocore_noclock: pico-sdk pico-extras lib/fuzz.h lib/transfer_saturate2.h lib/sinewaves2.h lib/crossfade4_441.h lib/resonantfilter_data.h lib/cuedsounds.h build
	cp lib/cmake/ezeptocore_compile_definitions_nooverclock.cmake lib/cmake/target_compile_definitions.cmake
	make -C build -j$(NPROCS)
	cp build/_core.uf2 ezeptocore.uf2

ezeptocore_noclock_128: pico-sdk pico-extras lib/fuzz.h lib/transfer_saturate2.h lib/sinewaves2.h lib/crossfade4_441.h lib/resonantfilter_data.h lib/cuedsounds.h build
	cp lib/cmake/ezeptocore_compile_definitions_nooverclock_128.cmake lib/cmake/target_compile_definitions.cmake
	make -C build -j$(NPROCS)
	cp build/_core.uf2 ezeptocore.uf2

ezeptocore_noclock_256: pico-sdk pico-extras lib/fuzz.h lib/transfer_saturate2.h lib/sinewaves2.h lib/crossfade4_441.h lib/resonantfilter_data.h lib/cuedsounds.h build
	cp lib/cmake/ezeptocore_compile_definitions_nooverclock_256.cmake lib/cmake/target_compile_definitions.cmake
	make -C build -j$(NPROCS)
	cp build/_core.uf2 ezeptocore.uf2

copyzepto:
	cp lib/cmake/zeptocore_compile_definitions.cmake lib/cmake/target_compile_definitions.cmake

copyecto:
	cp lib/cmake/ectocore_compile_definitions.cmake lib/cmake/target_compile_definitions.cmake

copyectonoclock:
	cp lib/cmake/ectocore_compile_definitions_nooverclock.cmake lib/cmake/target_compile_definitions.cmake

copyboard:
	cp lib/cmake/zeptoboard_compile_definitions.cmake lib/cmake/target_compile_definitions.cmake

envs:
	export PICO_EXTRAS_PATH=/home/zns/pico/pico-extras
	export PICO_SDK_PATH=/home/zns/pico/pico-sdk


wavetable: .venv
	cd lib && ../.venv/bin/python wavetable.py > wavetable_data.h
	clang-format -i --style=google lib/wavetable_data.h

lib/biquad.h: .venv
	cd lib && ../.venv/bin/python biquad.py > biquad.h

lib/fuzz.h: .venv
	cd lib && ../.venv/bin/python fuzz.py > fuzz.h
	clang-format -i --style=google lib/fuzz.h

lib/transfer_doublesine.h: .venv
	cd lib && ../.venv/bin/python transfer_doublesine.py > transfer_doublesine.h
	clang-format -i --style=google lib/transfer_doublesine.h


lib/sinewaves.h: .venv
	.venv/bin/python lib/sinewaves.py > lib/sinewaves.h
	clang-format -i lib/sinewaves.h

lib/sinewaves2.h: .venv
	.venv/bin/python lib/sinewaves2.py > lib/sinewaves2.h
	clang-format -i lib/sinewaves2.h

lib/crossfade4_441.h: .venv
	cd lib && ../.venv/bin/python crossfade4.py 64 > crossfade4_64.h
	clang-format -i --style=google lib/crossfade4_64.h
	cd lib && ../.venv/bin/python crossfade4.py 128 > crossfade4_128.h
	clang-format -i --style=google lib/crossfade4_128.h
	cd lib && ../.venv/bin/python crossfade4.py 160 > crossfade4_160.h
	clang-format -i --style=google lib/crossfade4_160.h
	cd lib && ../.venv/bin/python crossfade4.py 192 > crossfade4_192.h
	clang-format -i --style=google lib/crossfade4_192.h
	cd lib && ../.venv/bin/python crossfade4.py 256 > crossfade4_256.h
	clang-format -i --style=google lib/crossfade4_256.h
	cd lib && ../.venv/bin/python crossfade4.py 441 > crossfade4_441.h
	clang-format -i --style=google lib/crossfade4_441.h

lib/selectx2.h: .venv
	cd lib && ../.venv/bin/python selectx2.py > selectx2.h
	clang-format -i --style=google lib/selectx2.h

lib/crossfade.h: .venv lib/crossfade4_441.h
	cd lib && ../.venv/bin/python transfer_saturate.py > transfer_saturate.h
	clang-format -i --style=google lib/transfer_saturate.h
	cd lib && ../.venv/bin/python transfer_distortion.py > transfer_distortion.h
	clang-format -i --style=google lib/transfer_distortion.h
	cd lib && ../.venv/bin/python selectx.py > selectx.h
	clang-format -i --style=google lib/selectx.h
	cd lib && ../.venv/bin/python biquad.py > biquad.h
	clang-format -i --style=google lib/biquad.h
	cd lib && ../.venv/bin/python crossfade.py > crossfade.h
	clang-format -i --style=google lib/crossfade.h
	cd lib && ../.venv/bin/python crossfade2.py > crossfade2.h
	clang-format -i --style=google lib/crossfade2.h

lib/transfer_saturate2.h: .venv
	cd lib && ../.venv/bin/python transfer_saturate2.py > transfer_saturate2.h
	clang-format -i --style=google lib/transfer_saturate2.h

lib/resonantfilter_data.h: .venv
	cd lib && ../.venv/bin/python resonantfilter.py > resonantfilter_data.h
	clang-format -i --style=google lib/resonantfilter_data.h

lib/cuedsounds.h: lib/cuedsounds_zeptocore.h lib/cuedsounds_ectocore.h

lib/cuedsounds_zeptocore.h:
	cd dev/audio2flash && go build -v && ./audio2flash -name cuedsounds -in cuedsounds-zeptocore -out ../../lib/cuedsounds_zeptocore.h

lib/cuedsounds_ectocore.h:
	cd dev/audio2flash && go build -v && ./audio2flash -name cuedsounds -in cuedsounds-ectocore -out ../../lib/cuedsounds_ectocore.h

pico-extras:
	git clone --branch $(PICO_EXTRAS_VERSION) --depth 1 --recurse-submodules --shallow-submodules https://github.com/raspberrypi/pico-extras.git pico-extras

copysamples:
	cd dev/copysamples && go build -v
	# sudo XDG_RUNTIME_DIR=/run/user/1000 ./dev/copysamples/copysamples -src dev/starting_samples2
	# sudo XDG_RUNTIME_DIR=/run/user/1000 ./dev/copysamples/copysamples -src dev/starting_samples_kero
	#sudo XDG_RUNTIME_DIR=/run/user/1000 ./dev/copysamples/copysamples -src dev/pikocore-starting
	sudo XDG_RUNTIME_DIR=/run/user/1000 ./dev/copysamples/copysamples -src ~/Music/sdcard_zeptocore

minicom:
	cd dev/minicom && go build -v
	./dev/minicom/minicom

midicom:
	cd dev/midicom && go build -v
	./dev/midicom/midicom

changebaud:
	-curl localhost:7083

resetpico2:
	-timeout 1 sudo minicom -b 1200 -o -D /dev/ttyACM0
	-amidi -p $$(amidi -l | grep 'zeptocore\|zeptoboard\|ectocore' | awk '{print $$2}') -S "B00000"
	sleep 0.1
	-amidi -p $$(amidi -l | grep 'zeptocore\|zeptoboard\|ectocore' | awk '{print $$2}') -S "B00000"
	sleep 0.1
	-amidi -p $$(amidi -l | grep 'zeptocore\|zeptoboard\|ectocore' | awk '{print $$2}') -S "B00000"
	sleep 0.1

.PHONY: check_picotool enter_bootsel_1200 upload upload-built upload-legacy
check_picotool:
	@command -v "$(PICOTOOL)" >/dev/null 2>&1 || { \
		echo "picotool not found. Install it first, or run make upload PICOTOOL=/path/to/picotool"; \
		echo "macOS: brew install picotool"; \
		exit 1; \
	}

enter_bootsel_1200:
	@if [ "$(UPLOAD_TOUCH_1200)" != "1" ]; then \
		echo "Skipping 1200-baud BOOTSEL touch"; \
		exit 0; \
	fi
	python3 scripts/enter_bootsel_1200.py --baud "$(UPLOAD_TOUCH_BAUD)" --wait "$(UPLOAD_TOUCH_WAIT)" $(if $(filter 1,$(UPLOAD_MIDI_RESET)),--midi-fallback,--no-midi-fallback)

upload: dobuild
	$(MAKE) upload-built

# Upload an already-built artifact without reconfiguring firmware.
upload-built: check_picotool
	@test -f "$(UPLOAD_UF2)" || { echo "$(UPLOAD_UF2) does not exist; build failed or UPLOAD_UF2 is wrong"; exit 1; }
	$(MAKE) enter_bootsel_1200
	$(PICOTOOL) load $(PICOTOOL_LOAD_FLAGS) "$(UPLOAD_UF2)"

upload-legacy: resetpico2 changebaud dobuild
	./dev/upload.sh

bootreset: .venv dobuild
	.venv/bin/python dev/reset_pico.py /dev/ttyACM0

autoload: upload

.PHONY: check-pico-dependencies
check-pico-dependencies: pico-sdk pico-extras
	python3 scripts/check_pico_dependencies.py "$(PICO_SDK_PATH)" "$(PICO_SDK_VERSION)" "$(PICO_EXTRAS_PATH)" "$(PICO_EXTRAS_VERSION)"

.PHONY: build
build: check-pico-dependencies
	mkdir -p build
	cmake -S . -B build $(CORE_CMAKE_OPTIONS)
	make -C build -j$(NPROCS)
	echo "build success"

.PHONY: ensure_arm_toolchain build/Makefile
ensure_arm_toolchain:
ifeq ($(UNAME_S),Darwin)
	@set -e; \
	if ! command -v arm-none-eabi-gcc >/dev/null 2>&1 || [ "$$(arm-none-eabi-gcc -print-file-name=nosys.specs 2>/dev/null)" = "nosys.specs" ]; then \
		if [ ! -x "$(MACOS_ARM_TC_BIN)/arm-none-eabi-gcc" ]; then \
			echo "Installing local ARM GCC toolchain to $(MACOS_ARM_TC_DIR)"; \
			mkdir -p "$(HOME)/.cache/_core/toolchains"; \
			curl -fsSL "$(MACOS_ARM_TC_URL)" -o "$(HOME)/.cache/_core/toolchains/$(MACOS_ARM_TC_BASENAME).tar.gz"; \
			rm -rf "$(MACOS_ARM_TC_DIR)"; \
			tar -xzf "$(HOME)/.cache/_core/toolchains/$(MACOS_ARM_TC_BASENAME).tar.gz" -C "$(HOME)/.cache/_core/toolchains"; \
			rm -f "$(HOME)/.cache/_core/toolchains/$(MACOS_ARM_TC_BASENAME).tar.gz"; \
		fi; \
	fi
endif

build/Makefile: check-pico-dependencies
	mkdir -p build
	@set -e; \
	rm -f build/CMakeCache.txt; \
	if [ -x "$(MACOS_ARM_TC_BIN)/arm-none-eabi-gcc" ]; then \
		PATH="$(MACOS_ARM_TC_BIN):$$PATH" PICO_TOOLCHAIN_PATH="$(MACOS_ARM_TC_BIN)" cmake -S . -B build $(CORE_CMAKE_OPTIONS); \
	else \
		cmake -S . -B build $(CORE_CMAKE_OPTIONS); \
	fi

audio:
	sox lib/audio/amen_5c2d11c8_beats16_bpm170.flac -c 1 --bits 16 --encoding signed-integer --endian little amen_bpm170_beats16_mono.wav
	sox lib/audio/amen_0efedaab_beats8_bpm165.flac -c 1 --bits 16 --encoding signed-integer --endian little amen_bpm165_beats8_mono.wav

audio2:
	sox lib/audio/amen_5c2d11c8_beats16_bpm170.flac -c 2 --bits 16 --encoding signed-integer --endian little amen_bpm170_beats16_stereo.wav
	sox lib/audio/amen_0efedaab_beats8_bpm165.flac -c 2 --bits 16 --encoding signed-integer --endian little amen_bpm165_beats8_stereo.wav

bass: .venv
	cd lib && ../.venv/bin/python bass_raw.py audio/bass_e.wav bass_sample.h

# Keep SDKs, toolchains, virtual environments, captured data, and checked-in
# generated sources (such as sinewaves2.h and transfer_saturate2.h).
.PHONY: clean
clean:
	rm -rf build build-*/ test/*/build lib/test/*/build
	rm -rf visualizer/dist visualizer/.cache visualizer-juce/build-*/
	rm -rf docs/public
	rm -f *.uf2
	rm -f amen_bpm170_beats16_mono.wav amen_bpm165_beats8_mono.wav \
		amen_bpm170_beats16_stereo.wav amen_bpm165_beats8_stereo.wav
	rm -f lib/biquad.h lib/fuzz.h lib/transfer_doublesine.h \
		lib/selectx2.h lib/transfer_saturate.h lib/transfer_distortion.h \
		lib/selectx.h lib/crossfade.h lib/crossfade2.h lib/bass_sample.h \
		lib/crossfade4_64.h lib/crossfade4_128.h lib/crossfade4_160.h \
		lib/crossfade4_192.h lib/crossfade4_256.h lib/crossfade4_441.h \
		lib/resonantfilter_data.h lib/cuedsounds_zeptocore.h lib/cuedsounds_ectocore.h
	rm -f dev/audio2flash/audio2flash dev/gitread/gitread dev/bin2c/bin2c \
		dev/copysamples/copysamples dev/minicom/minicom dev/midicom/midicom
	rm -f core/core core_server core_windows.exe core_linux_amd64 \
		core_macos_aarch64 core_macos_amd64 core_macos_amd642 \
		ectocore_windows.exe ectocore_linux_amd64 ectocore_macos_aarch64 ectocore_macos_amd64

debug:
	sudo minicom -b 115200 -o -D /dev/ttyACM0

cloc:
	cloc --exclude-list-file=dev/.clocignore --exclude-lang="make,CMake,D,Markdown,JSON,INI,Bourne Shell,TOML,TypeScript,YAML,Assembly" *

ignore:
	git status --porcelain | grep '^??' | cut -c4- >> .gitignore
	git commit -am "update gitignore"

release:
	cd core && goreleaser release -p 32 --clean --snapshot

server:
	cd core && air

web: server

resetpico: .venv
	.venv/bin/python dev/reset_pico.py

versions.md:
	cd dev/gitread && go build -v && ./gitread ../../docs/content/versions/versions.md

docsbuild:
	cd docs && hugo --cleanDestinationDir --minify
	rm -rf core/src/server/docs
	cp -r docs/public core/src/server/docs

.PHONY: core_windows.exe
core_windows.exe: docsbuild
	cd core && CGO_ENABLED=1 CC="zig cc -target x86_64-windows-gnu" GOOS=windows GOARCH=amd64 go build -v -ldflags "-s -w" -o ../core_windows.exe

core/MacOSX11.3.sdk:
	cd core && wget -q https://github.com/joseluisq/macosx-sdks/releases/download/11.3/MacOSX11.3.sdk.tar.xz
	cd core && tar -xf MacOSX11.3.sdk.tar.xz

.PHONY: core_macos_aarch64
core_macos_aarch64: install_go21 docsbuild core/MacOSX11.3.sdk
	# https://web.archive.org/web/20230330180803/https://lucor.dev/post/cross-compile-golang-fyne-project-using-zig/
	cd core && MACOS_MIN_VER=11.3 MACOS_SDK_PATH=$(PWD)/core/MacOSX11.3.sdk CGO_ENABLED=1 GOOS=darwin GOARCH=arm64 \
	CGO_LDFLAGS="-mmacosx-version-min=$${MACOS_MIN_VER} --sysroot $${MACOS_SDK_PATH} -F/System/Library/Frameworks -L/usr/lib" \
	CC="zig cc -target aarch64-macos -isysroot $${MACOS_SDK_PATH} -iwithsysroot /usr/include -iframeworkwithsysroot /System/Library/Frameworks" \
	go1.21.13 build -ldflags "-s -w" -buildmode=pie -o ../core_macos_aarch64

.PHONY: core_macos_amd64
core_macos_amd64: install_go21 docsbuild core/MacOSX11.3.sdk
	# https://web.archive.org/web/20230330180803/https://lucor.dev/post/cross-compile-golang-fyne-project-using-zig/
	cd core && MACOS_MIN_VER=11.3 MACOS_SDK_PATH=$(PWD)/core/MacOSX11.3.sdk CGO_ENABLED=1 GOOS=darwin GOARCH=amd64 \
	CGO_LDFLAGS="-mmacosx-version-min=$${MACOS_MIN_VER} --sysroot $${MACOS_SDK_PATH} -F/System/Library/Frameworks -L/usr/lib" \
	CC="zig cc -target x86_64-macos -isysroot $${MACOS_SDK_PATH} -iwithsysroot /usr/include -iframeworkwithsysroot /System/Library/Frameworks" \
	go1.21.13 build -ldflags "-s -w" -buildmode=pie -o ../core_macos_amd64

.PHONY: core_macos_amd642
core_macos_amd642: docsbuild
	cd core && CGO_ENABLED=0 GOOS=darwin GOARCH=amd64 \
	go build -ldflags "-s -w" -buildmode=pie -o ../core_macos_amd642

.PHONY: core_linux_amd64
core_linux_amd64: docsbuild
	cd core && CGO_ENABLED=1 CC="zig cc -target x86_64-linux-gnu" GOOS=linux GOARCH=amd64 go build -ldflags "-s -w" -v -o ../core_linux_amd64

.PHONY: core_server
core_server: docsbuild
	cd core && go build -v -o ../core_server

# ectocore builds
.PHONY: ectocore_windows.exe
ectocore_windows.exe: docsbuild
	cd core && CGO_ENABLED=1 CC="zig cc -target x86_64-windows-gnu" GOOS=windows GOARCH=amd64 go build -v -ldflags "-s -w -X main.EctocoreDefault=yes" -o ../ectocore_windows.exe

.PHONY: ectocore_macos_aarch64
ectocore_macos_aarch64: install_go21 docsbuild core/MacOSX11.3.sdk
	# https://web.archive.org/web/20230330180803/https://lucor.dev/post/cross-compile-golang-fyne-project-using-zig/
	cd core && MACOS_MIN_VER=11.3 MACOS_SDK_PATH=$(PWD)/core/MacOSX11.3.sdk CGO_ENABLED=1 GOOS=darwin GOARCH=arm64 \
	CGO_LDFLAGS="-mmacosx-version-min=$${MACOS_MIN_VER} --sysroot $${MACOS_SDK_PATH} -F/System/Library/Frameworks -L/usr/lib" \
	CC="zig cc -target aarch64-macos -isysroot $${MACOS_SDK_PATH} -iwithsysroot /usr/include -iframeworkwithsysroot /System/Library/Frameworks" \
	go1.21.13 build -ldflags "-s -w -X main.EctocoreDefault=yes" -buildmode=pie -o ../ectocore_macos_aarch64

.PHONY: ectocore_macos_amd64
ectocore_macos_amd64: install_go21 docsbuild core/MacOSX11.3.sdk
	# https://web.archive.org/web/20230330180803/https://lucor.dev/post/cross-compile-golang-fyne-project-using-zig/
	cd core && MACOS_MIN_VER=11.3 MACOS_SDK_PATH=$(PWD)/core/MacOSX11.3.sdk CGO_ENABLED=1 GOOS=darwin GOARCH=amd64 \
	CGO_LDFLAGS="-mmacosx-version-min=$${MACOS_MIN_VER} --sysroot $${MACOS_SDK_PATH} -F/System/Library/Frameworks -L/usr/lib" \
	CC="zig cc -target x86_64-macos -isysroot $${MACOS_SDK_PATH} -iwithsysroot /usr/include -iframeworkwithsysroot /System/Library/Frameworks" \
	go1.21.13 build -ldflags "-s -w -X main.EctocoreDefault=yes" -buildmode=pie -o ../ectocore_macos_amd64

.PHONY: ectocore_linux_amd64
ectocore_linux_amd64: docsbuild
	cd core && CGO_ENABLED=1 CC="zig cc -target x86_64-linux-gnu" GOOS=linux GOARCH=amd64 go build -ldflags "-s -w -X main.EctocoreDefault=yes" -v -o ../ectocore_linux_amd64

.PHONY: docs
docs:
	cd docs && hugo serve -D --bind 0.0.0.0

.venv:
	uv venv --python 3.11
	uv pip install -r requirements.txt

dev/madmom/.venv:
	cd dev/madmom && uv venv
	cd dev/madmom && . .venv/bin/activate && uv pip install -r requirements.txt
