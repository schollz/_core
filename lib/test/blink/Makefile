dobuild: build
	cd build && make -j32

upload: dobuild
	./upload.sh 


build:
	rm -rf build
	mkdir build
	cd build && cmake ..
	cd build && make -j32
	echo "build success"

clean:
	rm -rf build