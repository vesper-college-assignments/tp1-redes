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

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
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

int setup_server(char **argv);

void receive_message(const ClientData *client, std::string &string_message) {
    char message_part[BUFSZ];
    size_t bytes_exchanged;
    char last_char;
    std::cout << "Receiving message" << std::endl;
    do{
        bytes_exchanged = recv(client->socket_descriptor, message_part, BUFSZ - 1, 0);
        std::cout << "Bytes recebidos: " << bytes_exchanged << std::endl;
        if(bytes_exchanged > 500){
            std::cout << "Msg maior que 500" << std::endl;
            close(client->socket_descriptor);
        }

        last_char = message_part[strlen(message_part) - 1];
        std::cout << (int)last_char << std::endl;
        string_message.append(message_part);
        memset(message_part, 0, BUFSZ);
    }while(last_char != '\n');

    std::cout << "Message complete" << std::endl;
}

void * client_thread(void *data) {
    auto *client = (ClientData *)data;
    auto *caddr = (struct sockaddr *)(&client->storage);

    char caddrstr[BUFSZ];
    addrtostr(caddr, caddrstr, BUFSZ);
    std::cout << "[log] incoming connection accepted"<< std::endl;

    while(true){
        std::string message_from_client;

        // ===== Receives message =========
        receive_message(client, message_from_client);
        std::cout << "Mensagem recebida: "<< message_from_client;

        for (const char& i : message_from_client){
            if (!isascii(message_from_client[i])){
                std::cout << "algo nao eh ascii" << std::endl;
                close(client->socket_descriptor);
                pthread_exit(nullptr);
            }
        }

         if(message_from_client == "##kill\n"){
            std::cout << "Recebeu mensagem de kill" << std::endl;
            exit(EXIT_SUCCESS);
        }

        // ==== Response =======
        char response[BUFSZ];
        memset(response, 0, BUFSZ);
        sprintf(response, "> resposta do servidor");

        size_t bytes_exchanged = send(client->socket_descriptor, response, strlen(response) + 1, 0);
        if (bytes_exchanged != strlen(response) + 1) {
            logexit("send");
        }
    }
}


#pragma clang diagnostic push
int main(int argc, char **argv) {
    if (argc < 3) {
        usage(argv);
    }

    int server_socket = setup_server(argv);

    while (true) {
        struct sockaddr_storage cstorage;
        auto *caddr = (struct sockaddr *)(&cstorage);
        socklen_t caddrlen = sizeof(cstorage);

        int client_socket = accept(server_socket, caddr, &caddrlen);
         std::cout << "nova conexão aceita no socket descriptor " << client_socket << std::endl;

        auto * new_client = new ClientData;
        new_client->socket_descriptor = client_socket;
        memcpy(&(new_client->storage), &cstorage, sizeof(cstorage));

        pthread_t tid;
        pthread_create(&tid, nullptr, client_thread, new_client);


    } // while
       
    exit(EXIT_SUCCESS);
}

int setup_server(char **argv) {
    struct sockaddr_storage server_data;
    if (0 != server_sockaddr_init(argv[1], argv[2], &server_data)) {
        usage(argv);
    }

    int s;

    s = socket(server_data.ss_family, SOCK_STREAM, 0);
    if (s == -1) {
        logexit("socket");
    }

    int enable = 1;
    if (0 != setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int))) {
        logexit("setsockopt");
    }

    auto *addr = (struct sockaddr *)(&server_data);
    if (0 != bind(s, addr, sizeof(server_data))) {
        logexit("binding error");
    }

    if (0 != listen(s, 10)) {
        logexit("listening error");
    }

    char addrstr[BUFSZ];
    addrtostr(addr, addrstr, BUFSZ);

    std::cout << "\"bound to "<<  addrstr  << " waiting connections" << std::endl;
    return s;
}

#pragma clang diagnostic pop
#pragma clang diagnostic pop