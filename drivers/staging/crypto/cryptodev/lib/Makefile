CFLAGS=-g -O2 -Wall -DDEBUG

all: benchmark

benchmark: benchmark.c hash.c main.c combo.c
	gcc $(CFLAGS) -o $@ $^ -lssl

clean:
	rm -f *.o *~ benchmark
