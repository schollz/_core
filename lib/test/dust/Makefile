
.PHONY: main	
main: main.o pcg_basic.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

pcg_basic.o:
	$(CC) $(CFLAGS) -c ../../pcg_basic.c

clean:
	rm -rf *.o
	rm -rf main