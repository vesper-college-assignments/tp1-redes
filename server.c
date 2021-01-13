#include "common.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/types.h>

#define BUFSZ 1024

void usage(int argc, char **argv) {
    printf("usage: %s <v4|v6> <server port>\n", argv[0]);
    printf("example: %s v4 51511\n", argv[0]);
    exit(EXIT_FAILURE);
}

struct client_data {
    int socket_descriptor;
    struct sockaddr_storage storage;
};


void * client_thread(void *data) {
    struct client_data *client = (struct client_data *)data;
    struct sockaddr *caddr = (struct sockaddr *)(&client->storage);

    char caddrstr[BUFSZ];
    addrtostr(caddr, caddrstr, BUFSZ);
    printf("[log] connection from %s\n", caddrstr);

    char message_received[BUFSZ];
    memset(message_received, 0, BUFSZ);
    int loops = 0;

    while(1){
        
        // ===== Receives message =========
        size_t count = recv(client->socket_descriptor, message_received, BUFSZ - 1, 0);
        printf("[msg] %s, %d bytes: %s\n", caddrstr, (int)count, message_received);
        
        // ==== Response =======
        char response[BUFSZ];
        memset(response, 0, BUFSZ);
        sprintf(response, "remote TESTE endpoint: %.100s\n", caddrstr);
        count = send(client->socket_descriptor, response, strlen(response) + 1, 0);
        if (count != strlen(response) + 1) {
            logexit("send");
        }

        // tratar msg recebida de kill
        int ret = strcmp(message_received, "##kill\n");
        if(ret == 0){
            exit(EXIT_SUCCESS);
            // TODO tem que dar free nas coisas?
        }
        memset(message_received, 0, BUFSZ);
    }

    close(client->socket_descriptor);

    pthread_exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        usage(argc, argv);
    }

    struct sockaddr_storage storage;
    if (0 != server_sockaddr_init(argv[1], argv[2], &storage)) {
        usage(argc, argv);
    }

    int s;
    s = socket(storage.ss_family, SOCK_STREAM, 0);
    if (s == -1) {
        logexit("socket");
    }

    int enable = 1;
    if (0 != setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int))) {
        logexit("setsockopt");
    }

    struct sockaddr *addr = (struct sockaddr *)(&storage);
    if (0 != bind(s, addr, sizeof(storage))) {
        logexit("binding error");
    }

    if (0 != listen(s, 10)) {
        logexit("listening error");
    }

    char addrstr[BUFSZ];
    addrtostr(addr, addrstr, BUFSZ);
    printf("bound to %s, waiting connections\n", addrstr);

    while (1) {
        struct sockaddr_storage cstorage;
        struct sockaddr *caddr = (struct sockaddr *)(&cstorage);
        socklen_t caddrlen = sizeof(cstorage);

        int csock = accept(s, caddr, &caddrlen);
        // printf("nova conexão aceit no socket descriptor %d\n", csock);
        // csock pode ser o meu ID do cliente


        // Se a conexão falhar, seria o caso de derrubar o server?
        if (csock == -1) {
            logexit("conexion not accepted");
        }

        struct client_data *new_client = malloc(sizeof(*new_client));
        if (!new_client) {
            logexit("malloc error for client data");
        }
        
        new_client->socket_descriptor = csock;
        memcpy(&(new_client->storage), &cstorage, sizeof(cstorage));

        pthread_t tid;
        pthread_create(&tid, NULL, client_thread, new_client);
    }
       
    exit(EXIT_SUCCESS);
}