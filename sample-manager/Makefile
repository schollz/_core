PRESET ?= $(shell if [ "$$(uname -s)" = Darwin ]; then if [ "$$(uname -m)" = arm64 ]; then echo macos-arm64; else echo macos-x86_64; fi; else echo linux-x86_64; fi)
JOBS ?= 6
VERSION ?= 0.1.0
LOG_FILE ?= $(CURDIR)/logs/sample-manager.log
RELEASE_ARGS ?=
MACOS_RELEASE_HOST ?= zns@192.168.0.44
BUILD := build/$(PRESET)
.PHONY: configure build run test package
configure:
	cmake --preset $(PRESET) -S . -DCORE_MANAGER_VERSION=$(VERSION)
build: configure
	cmake --build $(BUILD) --parallel $(JOBS)
test: build
	ctest --test-dir $(BUILD) --output-on-failure
run:
	@bash scripts/run-debug.sh "$(PRESET)" "$(JOBS)" "$(VERSION)" "$(LOG_FILE)"
package:
	python3 Release/package.py --platform $(PRESET) --version $(VERSION) --jobs $(JOBS)

# Release entrypoints clone main and use the latest release version/destination.
.PHONY: release-macosarm release-macos11 release-linux
release-macosarm:
	python3 Release/release-macosarm.py $(RELEASE_ARGS)
release-macos11:
	python3 Release/release-macos11.py "$(MACOS_RELEASE_HOST)" $(RELEASE_ARGS)
release-linux:
	python3 Release/release-linux.py $(RELEASE_ARGS)
