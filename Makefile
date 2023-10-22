dobuild: pico-extras lib/biquad.h build
	cd build && make -j32

lib/biquad.h:
	cd lib && python3 biquad.py > biquad.h


crossfade3:
	# set block size to 441
	cd lib && python3 crossfade3.py 441 > crossfade3.h
	clang-format -i --style=google lib/crossfade3.h 
 
libs: crossfade3
	cd lib && python3 transfer_saturate.py > transfer_saturate.h
	cd lib && python3 transfer_distortion.py > transfer_distortion.h
	cd lib && python3 selectx.py > selectx.h
	cd lib && python3 biquad.py > biquad.h
	cd lib && python3 crossfade.py > crossfade.h
	cd lib && python3 crossfade2.py > crossfade2.h


pico-extras:
	git clone https://github.com/raspberrypi/pico-extras.git
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
	cloc --by-file lib/*.h --exclude-list-file=dev/.clocignore
