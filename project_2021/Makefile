GCC = gcc
CFLAGS = -O3 -pthread 
CMAIN=main

all: main.o 
	$(GCC) $(CFLAGS) $^ -o $(CMAIN) -lm

%.o: %.c
	$(GCC) -c $(CFLAGS) $^

clean:
	rm -f *.o *~ $(CMAIN)