GCC = gcc
CFLAGS = -O3 -pthread 
CMAIN=problem

all: prod-cons.o 
	$(GCC) $(CFLAGS) $^ -o $(CMAIN) -lm

%.o: %.c
	$(GCC) -c $(CFLAGS) $^

clean:
	rm -f *.o *~ $(CMAIN)