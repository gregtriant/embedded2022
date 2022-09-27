all: client

client: client.cpp
	g++ client.cpp -I /usr/local/include -I ./ -l websockets -L /user/local/lib -o client

clean:
	rm -f client
	rm -rf client.dSYM