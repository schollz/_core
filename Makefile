dobuild: pico-extras lib/fuzz.h lib/transfer_saturate2.h lib/sinewaves2.h lib/crossfade3.h lib/resonantfilter_data.h build
	cd build && PICO_EXTRAS_PATH=../pico-extras make -j32

envs:
	export PICO_EXTRAS_PATH=/home/zns/pico/pico-extras 
	export PICO_SDK_PATH=/home/zns/pico/pico-sdk 

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

upload: dobuild
	./dev/upload.sh 

bootreset: dobuild
	python3 dev/reset_pico.py /dev/ttyACM0

autoload: dobuild bootreset upload

build: 
	rm -rf build
	mkdir build
	cd build && PICO_EXTRAS_PATH=../pico-extras cmake ..
	cd build && PICO_EXTRAS_PATH=../pico-extras make -j32
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