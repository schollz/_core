.DEFAULT_GOAL := build
ROOT := $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
CONFIG ?= $(if $(filter install,$(MAKECMDGOALS)),Release,Debug)
BUILD_DIR ?= $(ROOT)/build-$(shell echo $(CONFIG) | tr A-Z a-z)
JOBS ?= 4
CMAKE_FLAGS ?=
VST3_INSTALL_DIR ?= $(HOME)/Documents/vsts
PRODUCT := zeptocore visualizer
ARTEFACTS := $(BUILD_DIR)/zeptocore_visualizer_artefacts/$(CONFIG)
APP := $(ARTEFACTS)/Standalone/$(PRODUCT)
ifeq ($(shell uname -s),Darwin)
APP := $(APP).app/Contents/MacOS/$(PRODUCT)
endif
.PHONY: all configure build run vst3 au plugins install release test visual-check clean help
all: build
configure:
	cmake -S "$(ROOT)" -B "$(BUILD_DIR)" -G Ninja -DCMAKE_BUILD_TYPE=$(CONFIG) $(CMAKE_FLAGS)
build: configure
	cmake --build "$(BUILD_DIR)" --target zeptocore_visualizer_Standalone --parallel $(JOBS)
run: build
	"$(APP)" $(ARGS)
vst3: configure
	cmake --build "$(BUILD_DIR)" --target zeptocore_visualizer_VST3 --parallel $(JOBS)
au: configure
	cmake --build "$(BUILD_DIR)" --target zeptocore_visualizer_AU --parallel $(JOBS)
plugins: vst3
ifeq ($(shell uname -s),Darwin)
plugins: au
endif
install: vst3
	@test -d "$(ARTEFACTS)/VST3/$(PRODUCT).vst3"
	@if [ "$(ARTEFACTS)/VST3/$(PRODUCT).vst3" -ef "$(VST3_INSTALL_DIR)/$(PRODUCT).vst3" ]; then echo 'Install directory must differ from build output' >&2; exit 1; fi
ifeq ($(shell uname -s),Darwin)
	/usr/bin/codesign --verify --deep --strict "$(ARTEFACTS)/VST3/$(PRODUCT).vst3"
endif
	cmake -E make_directory "$(VST3_INSTALL_DIR)"
	cmake -E rm -rf "$(VST3_INSTALL_DIR)/$(PRODUCT).vst3"
	cmake -E copy_directory "$(ARTEFACTS)/VST3/$(PRODUCT).vst3" "$(VST3_INSTALL_DIR)/$(PRODUCT).vst3"
ifeq ($(shell uname -s),Darwin)
	/usr/bin/codesign --verify --deep --strict "$(VST3_INSTALL_DIR)/$(PRODUCT).vst3"
endif
release:
	$(MAKE) CONFIG=Release build plugins
test: build
	ctest --test-dir "$(BUILD_DIR)" --output-on-failure
visual-check: build
	"$(APP)" --render-fixtures "$(ROOT)/test-results/visuals"
clean:
	cmake --build "$(BUILD_DIR)" --target clean
help:
	@echo 'build / run / vst3 / au / plugins / release / test / visual-check / clean'
	@echo 'install: Release VST3 into ~/Documents/vsts (override VST3_INSTALL_DIR)'
	@echo 'CONFIG, BUILD_DIR, JOBS, CMAKE_FLAGS and ARGS are overridable'
