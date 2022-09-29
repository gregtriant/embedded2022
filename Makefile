all: client

client: client.cpp
	g++ -O3 -pthread client.cpp -I /usr/local/include -I ./ -l websockets -L /user/local/lib -o client -lboost_system -lboost_filesystem

clean:
	rm -f client
	rm -rf client.dSYM