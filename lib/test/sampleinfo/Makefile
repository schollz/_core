run: build
	./main

build:
	gcc -o main main.c -lm

check: build
	valgrind -s --leak-check=full --show-leak-kinds=all ./main