buildit: build
	cd build && make -j 32

build:
	mkdir -p build 
	cd build && cmake ..

clean:
	rm -rf build

upload: buildit
	./upload.sh 