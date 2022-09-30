all: client bin2txt

client: client.cpp
	g++ -O3 -pthread client.cpp -I /usr/local/include -I ./ -l websockets -L /user/local/lib -o client -lboost_system -lboost_filesystem

bin2txt: bin2txt.cpp
	g++ bin2txt.cpp -o bin2txt

clean:
	rm -f client
	rm -f bin2txt
	rm -rf client.dSYM