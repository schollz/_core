export PICO_EXTRAS_PATH ?= $(CURDIR)/pico-extras
export PICO_SDK_PATH ?= $(CURDIR)/pico-sdk

dobuild: pico-extras lib/fuzz.h lib/transfer_saturate2.h lib/sinewaves2.h lib/crossfade3.h lib/resonantfilter_data.h build
	make -C build -j32

zeptoboard: copyboard pico-extras lib/fuzz.h lib/transfer_saturate2.h lib/sinewaves2.h lib/crossfade3.h lib/resonantfilter_data.h build
	make -C build -j32
	mv build/_core.uf2 zeptoboard.uf2

zeptocore: copyzepto pico-extras lib/fuzz.h lib/transfer_saturate2.h lib/sinewaves2.h lib/crossfade3.h lib/resonantfilter_data.h build
	cp zeptocore_compile_definitions.cmake target_compile_definitions.cmake
	make -C build -j32
	mv build/_core.uf2 zeptocore.uf2

zeptocore_nooverclock: copyzepto pico-extras lib/fuzz.h lib/transfer_saturate2.h lib/sinewaves2.h lib/crossfade3.h lib/resonantfilter_data.h build
	cp zeptocore_compile_definitions.cmake target_compile_definitions.cmake
	sed -i 's/DO_OVERCLOCK=1/#DO_OVERCLOCK=1/g' target_compile_definitions.cmake
	make -C build -j32
	mv build/_core.uf2 zeptocore_nooverclock.uf2

ectocore: copyecto pico-extras lib/fuzz.h lib/transfer_saturate2.h lib/sinewaves2.h lib/crossfade3.h lib/resonantfilter_data.h build
	make -C build -j32
	mv build/_core.uf2 ectocore.uf2

copyzepto:
	cp zeptocore_compile_definitions.cmake target_compile_definitions.cmake

copyecto:
	cp ectocore_compile_definitions.cmake target_compile_definitions.cmake

copyboard:
	cp zeptoboard_compile_definitions.cmake target_compile_definitions.cmake

envs:
	export PICO_EXTRAS_PATH=/home/zns/pico/pico-extras 
	export PICO_SDK_PATH=/home/zns/pico/pico-sdk 


wavetable:
	cd lib && python3 wavetable.py > wavetable_data.h
	clang-format -i --style=google lib/wavetable_data.h

lib/biquad.h:
	cd lib && python3 biquad.py > biquad.h

lib/fuzz.h:
	cd lib && python3 fuzz.py > fuzz.h
	clang-format -i --style=google lib/fuzz.h

lib/transfer_doublesine.h:
	cd lib && python3 transfer_doublesine.py > transfer_doublesine.h
	clang-format -i --style=google lib/transfer_doublesine.h


lib/sinewaves.h:
	python3 lib/sinewaves.py > lib/sinewaves.h
	clang-format -i lib/sinewaves.h

lib/sinewaves2.h:
	python3 lib/sinewaves2.py > lib/sinewaves2.h
	clang-format -i lib/sinewaves2.h

lib/crossfade3.h:
	# set block size to 441
	cd lib && python3 crossfade3.py 441 > crossfade3.h
	clang-format -i --style=google lib/crossfade3.h 

lib/selectx2.h:
	cd lib && python3 selectx2.py > selectx2.h
	clang-format -i --style=google lib/selectx2.h
	
lib/crossfade.h: lib/crossfade3.h
	cd lib && python3 transfer_saturate.py > transfer_saturate.h
	clang-format -i --style=google lib/transfer_saturate.h 
	cd lib && python3 transfer_distortion.py > transfer_distortion.h
	clang-format -i --style=google lib/transfer_distortion.h 
	cd lib && python3 selectx.py > selectx.h
	clang-format -i --style=google lib/selectx.h 
	cd lib && python3 biquad.py > biquad.h
	clang-format -i --style=google lib/biquad.h 
	cd lib && python3 crossfade.py > crossfade.h
	clang-format -i --style=google lib/crossfade.h
	cd lib && python3 crossfade2.py > crossfade2.h
	clang-format -i --style=google lib/crossfade2.h 

lib/transfer_saturate2.h:
	cd lib && python3 transfer_saturate2.py > transfer_saturate2.h
	clang-format -i --style=google lib/transfer_saturate2.h

lib/resonantfilter_data.h:
	cd lib && python3 resonantfilter.py > resonantfilter_data.h
	clang-format -i --style=google lib/resonantfilter_data.h

pico-extras:
	git clone https://github.com/raspberrypi/pico-extras.git pico-extras
	cd pico-extras && git submodule update -i 

copysamples:
	cd dev/copysamples && go build -v 
	sudo XDG_RUNTIME_DIR=/run/user/1000 ./dev/copysamples/copysamples -src dev/starting_samples2

minicom:
	cd dev/minicom && go build -v
	./dev/minicom/minicom

midicom:
	cd dev/midicom && go build -v
	./dev/midicom/midicom
	
changebaud:
	curl localhost:7083 

resetpico2:
	-amidi -p $$(amidi -l | grep 'zeptocore\|zeptoboard' | awk '{print $$2}') -S "B00000"

upload: resetpico2 dobuild
	./dev/upload.sh 

bootreset: dobuild
	python3 dev/reset_pico.py /dev/ttyACM0

autoload: dobuild bootreset upload

build: 
	rm -rf build
	mkdir build
	cd build && cmake ..
	make -C build -j32
	echo "build success"

audio:
	sox lib/audio/amen_5c2d11c8_beats16_bpm170.flac -c 1 --bits 16 --encoding signed-integer --endian little amen_bpm170_beats16_mono.wav
	sox lib/audio/amen_0efedaab_beats8_bpm165.flac -c 1 --bits 16 --encoding signed-integer --endian little amen_bpm165_beats8_mono.wav

audio2:
	sox lib/audio/amen_5c2d11c8_beats16_bpm170.flac -c 2 --bits 16 --encoding signed-integer --endian little amen_bpm170_beats16_stereo.wav
	sox lib/audio/amen_0efedaab_beats8_bpm165.flac -c 2 --bits 16 --encoding signed-integer --endian little amen_bpm165_beats8_stereo.wav

bass:
	cd lib && python3 bass_raw.py audio/bass_e.wav bass_sample.h

clean:
	rm -rf build
	rm -rf *.wav
	rm -rf lib/biquad.h

debug:
	sudo minicom -b 115200 -o -D /dev/ttyACM

cloc:
	cloc --exclude-list-file=dev/.clocignore --exclude-lang="make,CMake,D,Markdown,JSON,INI,Bourne Shell,TOML,TypeScript,YAML,Assembly" *

ignore:
	git status --porcelain | grep '^??' | cut -c4- >> .gitignore
	git commit -am "update gitignore"

release:
	cd core && goreleaser release -p 32 --clean --snapshot

server:
	cd core && air

resetpico: 
	python3 dev/reset_pico.py 

docsbuild:
	cd docs && hugo
	rm -rf core/src/server/docs
	cp -r docs/public core/src/server/docs

.PHONY: core_windows.exe
core_windows.exe: docsbuild
	cd core && CGO_ENABLED=1 CC="zig cc -target x86_64-windows-gnu" GOOS=windows GOARCH=amd64 go build -v -ldflags "-s -w" -x -o ../core_windows.exe

core/MacOSX11.3.sdk:
	cd core && wget https://github.com/joseluisq/macosx-sdks/releases/download/11.3/MacOSX11.3.sdk.tar.xz
	cd core && tar -xvf MacOSX11.3.sdk.tar.xz

.PHONY: core_macos_aarch64
core_macos_aarch64: docsbuild core/MacOSX11.3.sdk
	# https://web.archive.org/web/20230330180803/https://lucor.dev/post/cross-compile-golang-fyne-project-using-zig/
	cd core && MACOS_MIN_VER=11.3 MACOS_SDK_PATH=$(PWD)/core/MacOSX11.3.sdk CGO_ENABLED=1 GOOS=darwin GOARCH=arm64 \
	CGO_LDFLAGS="-mmacosx-version-min=$${MACOS_MIN_VER} --sysroot $${MACOS_SDK_PATH} -F/System/Library/Frameworks -L/usr/lib" \
	CC="zig cc -target aarch64-macos -isysroot $${MACOS_SDK_PATH} -iwithsysroot /usr/include -iframeworkwithsysroot /System/Library/Frameworks" \
	go build -ldflags "-s -w" -buildmode=pie -v -x -o ../core_macos_aarch64

.PHONY: core_macos_amd64
core_macos_amd64: docsbuild core/MacOSX11.3.sdk
	# https://web.archive.org/web/20230330180803/https://lucor.dev/post/cross-compile-golang-fyne-project-using-zig/
	cd core && MACOS_MIN_VER=11.3 MACOS_SDK_PATH=$(PWD)/core/MacOSX11.3.sdk CGO_ENABLED=1 GOOS=darwin GOARCH=amd64 \
	CGO_LDFLAGS="-mmacosx-version-min=$${MACOS_MIN_VER} --sysroot $${MACOS_SDK_PATH} -F/System/Library/Frameworks -L/usr/lib" \
	CC="zig cc -target x86_64-macos -isysroot $${MACOS_SDK_PATH} -iwithsysroot /usr/include -iframeworkwithsysroot /System/Library/Frameworks" \
	go build -ldflags "-s -w" -buildmode=pie -v -x -o ../core_macos_amd64

.PHONY: core_macos_amd642
core_macos_amd642: docsbuild
	cd core && CGO_ENABLED=0 GOOS=darwin GOARCH=amd64 \
	go build -ldflags "-s -w" -buildmode=pie -v -x -o ../core_macos_amd642

.PHONY: core_linux_amd64
core_linux_amd64: docsbuild
	cd core && CGO_ENABLED=1 CC="zig cc -target x86_64-linux-gnu" GOOS=linux GOARCH=amd64 go build -ldflags "-s -w" -v -x -o ../core_linux_amd64

.PHONY: docs
docs:
	cd docs && hugo serve -D