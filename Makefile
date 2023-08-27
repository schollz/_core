dobuild: pico-extras lib/biquad.h build
	cd build && PICO_EXTRAS_PATH=../pico-extras make -j4

lib/biquad.h:
	cd lib && python3 biquad.py > biquad.h

libs:
	cd lib && python3 transfer_saturate.py > transfer_saturate.h
	cd lib && python3 transfer_distortion.py > transfer_distortion.h
	cd lib && python3 selectx.py > selectx.h
	cd lib && python3 biquad.py > biquad.h
	cd lib && python3 crossfade.py > crossfade.h


pico-extras:
	git clone https://github.com/raspberrypi/pico-extras.git
	cd pico-extras && git submodule update -i 

upload: dobuild
	./dev/upload.sh 

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

clean:
	rm -rf build
	rm -rf *.wav
	rm -rf lib/biquad.h

