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
#include <unordered_map>
#include <set>
#include <sstream>

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"


#define BUFSZ 501
/*
global hash tags_to_subscribers (string: set(client_socket));
global set subscribers_sockets
*/

static pthread_mutex_t  mutex = PTHREAD_MUTEX_INITIALIZER;
std::unordered_map<std::string, std::set<int>> tags_to_subscribers;
std::set<int> send_message_to;


class ClientData {
public:
    int socket_descriptor;
    struct sockaddr_storage storage;
};

void answer_client(int socket_descriptor, const char *answer) {
    char response[BUFSZ];
    memset(response, 0, BUFSZ);
    sprintf(response, "%s", answer);

    size_t bytes_exchanged = send(socket_descriptor, response, strlen(response) + 1, 0);
    if (bytes_exchanged != strlen(response) + 1) {
        logexit("send");
    }
}

void subscribe(int socket_descriptor, const std::string& tag) {
    pthread_mutex_lock(&mutex);

    // If client not subscribed to tag
    if (tags_to_subscribers[tag].find(socket_descriptor) == tags_to_subscribers[tag].end()) {
        answer_client(socket_descriptor, "subscribed");
        tags_to_subscribers[tag].insert(socket_descriptor);
    } else {
        answer_client(socket_descriptor, ">already subscribed");
    }
    pthread_mutex_unlock(&mutex);

}

void unsubscribe(int socket_descriptor, const std::string& tag){
    pthread_mutex_lock(&mutex);

    // I client subscribes to tag, unsubscribe it it
    std::set<int>::iterator it;
    it = tags_to_subscribers[tag].find(socket_descriptor);
    if(it != tags_to_subscribers[tag].end()){
        answer_client(socket_descriptor, "unsubscribed");
        tags_to_subscribers[tag].erase(it);
    } else{
        answer_client(socket_descriptor, "not subscribed");
    }

    pthread_mutex_unlock(&mutex);
}

void usage(char **port) {
    std::cout << "usage: "<<  port[0]  << " <v4|v6> <server port>" << std::endl;
    std::cout << "example: " << port[0] << " v4 51511" << std::endl;
    exit(EXIT_FAILURE);
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

std::string receive_message(const ClientData *client) {
    std::string message_from_client;
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
        message_from_client.append(message_part);
        memset(message_part, 0, BUFSZ);
    }while(last_char != '\n');

    std::cout << "Message complete" << std::endl;
    return message_from_client;
}

std::set<std::string> get_tags(std::string message){
    std::istringstream message_stream(message);
    std::string token;
    std::set<std::string> tags;
    while(message_stream){
        message_stream >> token;
        if(token[0] == '#'){
            tags.insert(token); //TODO tem que inserir sem a #
        }
    }
    std::cout << tags.size() << std::endl;
}


std::set<int> get_subscribers(std::set<std::string> tags){
    std::set<int> subscribers;
    for(auto tag : tags){
        for(auto sub : tags_to_subscribers[tag]) {
            subscribers.insert(sub);
        }
    }
    return subscribers;
}

void distribute_messages(std::set<int> subscribers){

}
void * client_thread(void *data) {
    auto *client = (ClientData *)data;
    auto *caddr = (struct sockaddr *)(&client->storage);
    char caddrstr[BUFSZ];
    addrtostr(caddr, caddrstr, BUFSZ);

    while(true){
        // Receive message
        std::string message_from_client;
        message_from_client = receive_message(client);

        // Check input format
        for (const char& i : message_from_client){
            if (!isascii(message_from_client[i])){
                std::cout << "algo nao eh ascii" << std::endl;
                close(client->socket_descriptor);
                pthread_exit(nullptr);
            }
        }
        std::cout << "Mensagem recebida: "<< message_from_client;

        // Check for kill
        if(message_from_client == "##kill\n"){
            std::cout << "Recebeu mensagem de kill" << std::endl;
            exit(EXIT_SUCCESS);
        }

        // Decide action
        char first_char = message_from_client[0];
        switch (first_char) {
            case '+': {
                std::string tag = message_from_client.substr(1);
                subscribe(client->socket_descriptor, tag);
                break;
            }
            case '-': {
                std::string tag = message_from_client.substr(1);
                unsubscribe(client->socket_descriptor, tag);
                break;
            }
            default: {
                std::set<std::string> tags = get_tags(message_from_client);
                if (!tags.empty()){
                    std::set<int> subscribers = get_subscribers(tags);
                    distribute_messages(subscribers);
                }
            }
        }// switch
    } // while
}// client_thread




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

        // Process new connection
        int client_socket = accept(server_socket, caddr, &caddrlen);
        auto * new_client = new ClientData;
        new_client->socket_descriptor = client_socket;
        memcpy(&(new_client->storage), &cstorage, sizeof(cstorage));
        std::cout << "nova conexão aceita no socket descriptor " << client_socket << std::endl;

        // Start a thread for new client
        pthread_t tid;
        pthread_create(&tid, nullptr, client_thread, new_client);


    } // while
       
    exit(EXIT_SUCCESS);
}



#pragma clang diagnostic pop
#pragma clang diagnostic pop