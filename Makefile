all:
	g++ -Wall -c common.cc
	g++ -Wall client.cc common.o -lpthread -o client
	g++ -Wall server.cc common.o -lpthread -o server

clean:
	rm common.o client server server
