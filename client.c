#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define BUFSZ 1024

struct sock {
  int csock;
};

void usage(int ip_address, char **port) {
	printf("usage: %s <server IP> <server port>\n", port[0]);
	printf("example: %s 127.0.0.1 51511\n", port[0]);
	exit(EXIT_FAILURE);
}


void *listenkb(void *data) {
  while (1) {
    
    // Aquie ele tá castando o pointer para um struct*?
    // Pq não recebeu o pointer especifico?
    struct sock *sdata = (struct sock *)data;
    char buf[BUFSZ];
    memset(buf, 0, BUFSZ);

    printf("> ");
    fgets(buf, BUFSZ - 1, stdin);
    size_t count = send(sdata->csock, buf, strlen(buf) + 1, 0);
    if (count != strlen(buf) + 1) {
      logexit("send");
    }
  }

  pthread_exit(EXIT_SUCCESS);
}

void *listennw(void *data) {
  struct sock *sdata = (struct sock *)data;
  while (1) {
    char buf[BUFSZ];
    memset(buf, 0, BUFSZ);

    size_t count = recv(sdata->csock, buf, BUFSZ, 0);
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("\r< %s> ", buf);
    if (count == 0) {
      break;
    }
  }

  close(sdata->csock);
  sdata->csock = -1;
  pthread_exit(EXIT_SUCCESS);
}



int main(int argc, char **argv) {
	/*
	argv[1] => server ip address
	argv[2] => server port
	o client address é enviado automaticamente
	o client port é decidido pelo SO e enviado automaticamente
	*/
	if (argc < 3) {
		usage(argc, argv);
	}

	// Aqui serão armazenadas as info do server socket
	struct sockaddr_storage server_socket; /* Structure large enough to hold any socket address*/
	
	// Aqui estamos preenchendo a struc que segura os dados do server socket.
	// Precisamos dessa função pq não sabemos se é ipv4 ou 6 e essa função faz essa crítica
	if (0 != client_sockaddr_init(argv[1], argv[2], &server_socket)) {
		usage(argc, argv);
	}
	
	// abrindo um client socket
	// int sockfd = socket(domain(ipv4 ou 6), type, protocol)
	// sockfd: socket descriptor, an integer (like a file-handle)
	int client_socket_descriptor;
	client_socket_descriptor = socket(server_socket.ss_family, SOCK_STREAM, 0);
	if (client_socket_descriptor == -1) {
		logexit("socket not created");
	}

	/*Tentando conexão*/
	
	// Onde essa struct é usada? Essa struct é uma interface e precisa ser castado
	// Pegou um ponteiro 
	// o connect liga 2 sockets
	// pq aqui não precisa ser do tipo storage? Pq tem que ser o que a função connect pede
	
	struct sockaddr *server_ip_address = (struct sockaddr *)(&server_socket); /* Structure describing a generic socket address.  */
	if (0 != connect(client_socket_descriptor, server_ip_address, sizeof(server_socket))) {
		logexit("connect failed");
	}

	// Imprime o endereço ao qual está conectado
	char addrstr[BUFSZ];
	addrtostr(server_ip_address, addrstr, BUFSZ);
	printf("connected to %s\n", addrstr);

	// pq um struct e não apenas o fd?????
	struct sock *sdata = malloc(sizeof(*sdata));
	sdata->csock = client_socket_descriptor;
	// sdata tem o client_socket_descriptor


	// Ele tá passando o mesmo socket_descriptor para ambas as threads
	// É possível usar a mesma port para ouvir processos diferentes??

	// Listen to keyboard
	pthread_t kbtid;
	pthread_create(&kbtid, NULL, listenkb, sdata);

	// Listen to network
	pthread_t nwtid;
	pthread_create(&nwtid, NULL, listennw, sdata);

	while (1) {
		if (sdata->csock == -1) {
		printf("\r[Connection] Disconnected by %s\n", addrstr);
		break;
		}
	}



	exit(EXIT_SUCCESS);
}