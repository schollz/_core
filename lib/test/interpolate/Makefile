plot:
	gcc -o main main.c -lm
	# ./main | gnuplot -p -e 'set datafile separator ","; set multiplot; plot "/dev/stdin" using 1:2 with lines'
	./main | python3 plot.py