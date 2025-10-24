# Makefile for GPIO_INPUTDETECT test
export PICO_SDK_PATH=$(CURDIR)/pico-sdk
NPROCS := $(shell grep -c 'processor' /proc/cpuinfo)

.PHONY: all build upload clean resetpico2 changebaud

all: pico-sdk buildit

pico-sdk:
	@if [ ! -d "pico-sdk" ]; then \
		echo "Cloning pico-sdk..."; \
		git clone https://github.com/raspberrypi/pico-sdk && \
		cd pico-sdk && git checkout 2.1.1 && \
		git submodule update --init --recursive; \
	else \
		echo "pico-sdk already exists"; \
	fi

build:
	@echo "Building input_detection_test..."
	mkdir -p build

buildit: build pico-sdk
	cd build && cmake ..
	make -C build -j$(NPROCS)
	@echo "Build success: build/input_detection_test.uf2"

upload: resetpico2 changebaud build
	@echo "Uploading to device..."
	@if [ -d "/media/$(USER)/RP2040" ]; then \
		cp build/input_detection_test.uf2 /media/$(USER)/RP2040/; \
		echo "Uploaded to /media/$(USER)/RP2040/"; \
	elif [ -d "/media/$(USER)/RPI-RP2" ]; then \
		cp build/input_detection_test.uf2 /media/$(USER)/RPI-RP2/; \
		echo "Uploaded to /media/$(USER)/RPI-RP2/"; \
	else \
		echo "Error: Pico not found in bootloader mode"; \
		echo "Please manually copy build/input_detection_test.uf2 to your Pico"; \
		exit 1; \
	fi

resetpico2:
	-amidi -p $$(amidi -l | grep 'zeptocore\|zeptoboard\|ectocore' | awk '{print $$2}') -S "B00000"
	sleep 0.1
	-amidi -p $$(amidi -l | grep 'zeptocore\|zeptoboard\|ectocore' | awk '{print $$2}') -S "B00000"
	sleep 0.1
	-amidi -p $$(amidi -l | grep 'zeptocore\|zeptoboard\|ectocore' | awk '{print $$2}') -S "B00000"
	sleep 0.1

changebaud:
	-curl localhost:7083

clean:
	rm -rf build

cleanall: clean
	rm -rf pico-sdk

debug:
	sudo minicom -b 115200 -o -D /dev/ttyACM0
