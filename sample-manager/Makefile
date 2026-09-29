PRESET ?= $(shell if [ "$$(uname -s)" = Darwin ]; then if [ "$$(uname -m)" = arm64 ]; then echo macos-arm64; else echo macos-x86_64; fi; else echo linux-x86_64; fi)
JOBS ?= 6
VERSION ?= 0.1.0
LOG_FILE ?= $(CURDIR)/logs/sample-manager.log
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
