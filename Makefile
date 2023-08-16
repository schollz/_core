dobuild: pico-extras build
	cd lib && python3 biquad.py > biquad.h
	cd build && PICO_EXTRAS_PATH=pico-extras make

pico-extras:
	git clone https://github.com/raspberrypi/pico-extras.git
	cd pico-extras && git submodule update -i 

upload: dobuild
	./dev/upload.sh 

build:
	rm -rf build
	mkdir build
	cd build && cmake ..
	cd build && make -j32
	echo "build success"

audio:
	sox lib/audio/amen_5c2d11c8_beats16_bpm170.flac -c 1 --bits 16 --encoding signed-integer --endian little amen_bpm170.wav
	sox lib/audio/amen_0efedaab_beats8_bpm165.flac -c 1 --bits 16 --encoding signed-integer --endian little amen_bpm165.wav

clean:
	rm -rf build
	rm -rf *.wav

