buildit: build
	cd build && make -j 4

resetminicom:
	curl localhost:7083 &

build:
	mkdir -p build 
	cd build && cmake ..

clean:
	rm -rf build

upload: resetminicom buildit
	./upload.sh 