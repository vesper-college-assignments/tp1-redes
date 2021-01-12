#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define BUFSZ 1024

/* Shown when usr want to know how to use*/
void usage(int ip_address, char **port) {
	printf("usage: %s <server IP> <server port>\n", port[0]);
	printf("example: %s 127.0.0.1 51511\n", port[0]);
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
	/*se menor que 3 é pq usou errado?*/
	if (argc < 3) {
		usage(argc, argv);
	}

	// struct grandão que a gente vai fazer o cast para a struct adequada
    // ele serve pra ser genérico entre o ipv4 ou ipv6
	// addpaser inicializa a struc. Se der errado, passram argumentos errados
	struct sockaddr_storage storage;
	if (0 != client_sockaddr_init(argv[1], argv[2], &storage)) {
		usage(argc, argv);
	}
	
	// abrindo um socket
	// int sockfd = socket(domain, type, protocol)
	// sockfd: socket descriptor, an integer (like a file-handle)
	int sock;
	sock = socket(storage.ss_family, SOCK_STREAM, 0);
	if (sock == -1) {
		logexit("socket not created");
	}

	/*Tentando conexão*/
	
	// Onde essa struct é usada?Essa struct é uma interface e precisa ser castado
	// Pegou um ponteiro 
	struct sockaddr *addr = (struct sockaddr *)(&storage);
	if (0 != connect(sock, addr, sizeof(storage))) {
		logexit("connect failed");
	}

	// Imprime o enderço ao qual está conectado
	char addrstr[BUFSZ];
	addrtostr(addr, addrstr, BUFSZ);
	printf("connected to %s\n", addrstr);





	// ===== Aqui manda a mensagem =====
	char message[BUFSZ];
	memset(message, 0, BUFSZ);
	
	printf("mensagem> ");
	 
	 //lê a msg do teclado
	fgets(message, BUFSZ-1, stdin);

	// send(pra onde, manda o que, flags). Retorna: a qtd de bytes transmitidos
	size_t count = send(sock, message, strlen(message)+1, 0);
	if (count != strlen(message)+1) {
		logexit("sent a different qtt of bytes");
	}

	// ===== Recebe a resposta do servidor ======
	memset(message, 0, BUFSZ);
	unsigned total = 0;
	
	// vai recebendo os pacotes até a conexão fechar
	// se essa conexão
	while(1) {
		count = recv(sock, message + total, BUFSZ - total, 0);
		if (count == 0) {
			// Connection terminated.
			break;
		}
		total += count;
	}
	
	// Tá fechando aqui pq esse exemplos ó envia e recebe uma msg
	// no tp, só dá esse close qnd especificado
	close(sock);

	printf("received %u bytes\n", total);
	puts(message);

	exit(EXIT_SUCCESS);
}