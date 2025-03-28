run: build
	./main 

build:
	gcc -o main main.c -lm

check: build
	valgrind ./main