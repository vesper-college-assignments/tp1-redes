#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <ostream>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define BUFSZ 501

class SocketWrapper {
public:
    int socket_descriptor;
};

void usage(char **port) {
	std::cout << "usage: "<<  port[0]  << " <server IP> <server port>" << std::endl;
    std::cout << "example: " << port[0] << " 127.0.0.1 51511" << std::endl;
	exit(EXIT_FAILURE);
}


[[noreturn]] void *stdin_thread(void *data) {
    while (true) {
        auto *sdata = (SocketWrapper *) data;

        char message_to_send[BUFSZ];
        memset(message_to_send, 0, BUFSZ);

        fgets(message_to_send, BUFSZ - 1, stdin);

        size_t count = send(sdata->socket_descriptor, message_to_send, strlen(message_to_send) + 1, 0);
        if (count != strlen(message_to_send) + 1) {
            logexit("send");
        }
    }
}

void *network_thread(void *data) {
    auto *sdata = (SocketWrapper *) data;

    while (true) {
        char message_received[BUFSZ];
        memset(message_received, 0, BUFSZ);

        size_t total_bytes = recv(sdata->socket_descriptor, message_received, BUFSZ, 0);
        setvbuf(stdout, NULL, _IONBF, 0);
        std::cout << message_received << std::endl;

        if (total_bytes == 0) {
            break;
        }
    }

    close(sdata->socket_descriptor);
    sdata->socket_descriptor = -1;
    pthread_exit(EXIT_SUCCESS);
}


int main(int argc, char **argv) {

	if (argc < 3) {
		usage(argv);
	}

	// Aqui serão armazenadas as info do server socket
	struct sockaddr_storage server_socket; /* Structure large enough to hold any socket address*/
	if (0 != client_sockaddr_init(argv[1], argv[2], &server_socket)) {
		usage(argv);
	}

	// abrindo um client socket
	int client_socket_descriptor;
	client_socket_descriptor = socket(server_socket.ss_family, SOCK_STREAM, 0);
	if (client_socket_descriptor == -1) {
		logexit("socket not created");
	}

	/*Tentando conexão*/
    auto *server_ip_address = (struct sockaddr *)(&server_socket); /* Structure describing a generic socket address.  */
	if (0 != connect(client_socket_descriptor, server_ip_address, sizeof(server_socket))) {
		logexit("connect failed");
	}

	// Imprime o endereço ao qual está conectado
	char addrstr[BUFSZ];
	addrtostr(server_ip_address, addrstr, BUFSZ);

    std::cout << "connected to " << addrstr << std::endl;

	auto * sdata = new SocketWrapper;
	sdata->socket_descriptor = client_socket_descriptor;

	// Listen to keyboard
	pthread_t std_in;
    int err_client = pthread_create(&std_in, NULL, stdin_thread, sdata);
    if (err_client) { std::cout << "Thread creation failed : " << strerror(err_client); }
    else { std::cout << "Stdin thread Created with ID : " << std_in << std::endl; }

	// Listen to network
	pthread_t network;
    int err_ntw = pthread_create(&network, NULL, network_thread, sdata);
    if (err_ntw) { std::cout << "Thread creation failed : " << strerror(err_ntw); }
    else { std::cout << "Network thread Created with ID : " << std_in << std::endl; }

	while (true) {
		if (sdata->socket_descriptor == -1) {
			std::cout << "\r[Connection] Disconnected by server"<< std::endl;
		    break;
		}
	}

	exit(EXIT_SUCCESS);
}