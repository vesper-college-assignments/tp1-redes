all:
	g++ -Wall -c common.cc
	g++ -Wall client.cc common.o -lpthread -o cliente
	g++ -Wall server.cc common.o -lpthread -o servidor

clean:
	rm common.o cliente servidor
