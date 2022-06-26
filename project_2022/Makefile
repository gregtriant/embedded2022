all: client

client: client.c
	gcc -g -Wall $< -o $@ `pkg-config libwebsockets --libs --cflags`
	
clean:
	rm -f client
	rm -rf client.dSYM
