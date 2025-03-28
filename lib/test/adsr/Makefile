plot:
	gcc -o main main.c -lm
	./main  | python3 plot.py

check:
	gcc -o main main.c -lm 
	valgrind ./main

clean:
	rm -rf *.o
	rm -rf main