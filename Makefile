dobuild: pico-extras lib/biquad.h build
	cd lib && python3 transfer_doublesine.py > transfer_doublesine.h
	cd lib && python3 transfer_tanh.py > transfer_tanh.h
	cd build && PICO_EXTRAS_PATH=../pico-extras make

lib/biquad.h:
	cd lib && python3 biquad.py > biquad.h

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
	sox lib/audio/amen_5c2d11c8_beats16_bpm170.flac -c 1 --bits 16 --encoding signed-integer --endian little amen_bpm170_beats16.wav
	sox lib/audio/amen_0efedaab_beats8_bpm165.flac -c 1 --bits 16 --encoding signed-integer --endian little amen_bpm165_beats8.wav

clean:
	rm -rf build
	rm -rf *.wav
	rm -rf lib/biquad.h

