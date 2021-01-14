#include "common.h"

#include <pthread.h>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <cctype>
#include <ostream>
#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <cstring>
#include <vector>

#define BUFSZ 501

void usage(char **port) {
    std::cout << "usage: "<<  port[0]  << " <v4|v6> <server port>" << std::endl;
    std::cout << "example: " << port[0] << " v4 51511" << std::endl;
    exit(EXIT_FAILURE);
}

class ClientData {
public:
    int socket_descriptor;
    struct sockaddr_storage storage;
};


int validate_input(char * message_received, size_t bytes_received){

    // If kill message
    int ret = strcmp(message_received, "##kill\n");
    if(ret == 0){
        std::cout << "Recebeu mensagem de kill" << std::endl;
        exit(EXIT_SUCCESS);
        // TODO tem que dar free nas coisas?
    }

    // if greater than 500 bytes
    if(bytes_received > 500){
        std::cout << "Msg maior que 500" << std::endl;
        return -1;
    }

    // is ascii
    // TODO passar pra cpp
//    for (int i = 0; i < strlen(message_received); i++){
//        if (!isascii(message_received[i])){
//        std::cout << "algo nao eh ascii" << std::endl;
//            return -1;
//        }
//    }
    return 0;
}

int is_finished(char * message_received){
    int len = strlen(message_received);
    if (message_received[len-1] == '\n'){
        return 1;
    }
    return 0;
}

void * client_thread(void *data) {
    auto *client = (ClientData *)data;
    auto *caddr = (struct sockaddr *)(&client->storage);

    char caddrstr[BUFSZ];
    addrtostr(caddr, caddrstr, BUFSZ);
    std::cout << "[log] connection from " << caddrstr << std::endl;
    char message_received[BUFSZ];

    while(true){

        // ===== Receives message =========
//        size_t bytes_exchanged = recv(client->socket_descriptor, message_received, BUFSZ - 1, 0);

        // TESTE DE RECEBER VARIAS =========================================================
        std::vector<char> buffer(BUFSZ);
        std::string rcv;
        size_t bytesReceived = 0;
        do {
            bytesReceived = recv(client->socket_descriptor, &buffer[0], buffer.size(), 0);
            // append string from buffer.
            if(0 != bytesReceived) {
                // error
            } else {
                rcv.append( buffer.cbegin(), buffer.cend() );
            }
        } while ( bytesReceived == BUFSZ );
        // ====================================================================

        std::cout << "[msg] "<< caddrstr << ", " << (int)bytesReceived << " bytes: " << message_received << std::endl;

        if(0 != validate_input(message_received, bytesReceived)){

            std::cout << "Bad input. Dumping client" << std::endl;

            close(client->socket_descriptor);
            pthread_exit(nullptr);
        }

        if(is_finished(message_received)){
            std::cout << "Mensagem contem \\n" <<  std::endl;
        }

        std::cout << "tamanho da msg recebida: " << (int)strlen(message_received) << std::endl;


        memset(message_received, 0, BUFSZ);

        // ==== Response =======
        char response[BUFSZ];
        memset(response, 0, BUFSZ);
        sprintf(response, "remote endpoint: %.100s\n", caddrstr);
        bytesReceived = send(client->socket_descriptor, response, strlen(response) + 1, 0);
        if (bytesReceived != strlen(response) + 1) {
            logexit("send");
        }
    }
}

#pragma clang diagnostic push
int main(int argc, char **argv) {
    if (argc < 3) {
        usage(argv);
    }

    struct sockaddr_storage storage;
    if (0 != server_sockaddr_init(argv[1], argv[2], &storage)) {
        usage(argv);
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

    auto *addr = (struct sockaddr *)(&storage);
    if (0 != bind(s, addr, sizeof(storage))) {
        logexit("binding error");
    }

    if (0 != listen(s, 10)) {
        logexit("listening error");
    }

    char addrstr[BUFSZ];
    addrtostr(addr, addrstr, BUFSZ);

    std::cout << "\"bound to "<<  addrstr  << " waiting connections" << std::endl;

    while (true) {
        struct sockaddr_storage cstorage;
        struct sockaddr *caddr = (struct sockaddr *)(&cstorage);
        socklen_t caddrlen = sizeof(cstorage);

        int csock = accept(s, caddr, &caddrlen);
         std::cout << "nova conexão aceita no socket descriptor " << socket_descriptor << std::endl;

        auto * new_client = new ClientData;
        new_client->socket_descriptor = csock;
        memcpy(&(new_client->storage), &cstorage, sizeof(cstorage));

        pthread_t tid;
        pthread_create(&tid, nullptr, client_thread, new_client);


    }
       
    exit(EXIT_SUCCESS);
}
#pragma clang diagnostic pop