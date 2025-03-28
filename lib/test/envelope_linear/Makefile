plot:
	gcc -o main main.c -lm
	./main  | gnuplot -p -e 'plot "/dev/stdin"  using 0:1 with lines'