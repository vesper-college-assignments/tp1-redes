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

#define BUFSZ 501


static pthread_mutex_t  mutex = PTHREAD_MUTEX_INITIALIZER;
std::unordered_map<std::string, std::set<int>> tags_to_subscribers;
std::unordered_map<int, std::set<std::string>> subscribers_tags;

class ClientData {
public:
    int socket_descriptor;
    struct sockaddr_storage storage;
};

void answer_client(int socket_descriptor, const char *answer) {
    std::cout << "Enviado mensagem ao cliente " << socket_descriptor << std::endl;
    size_t bytes_exchanged = send(socket_descriptor, answer, strlen(answer) + 1, 0);
    if (bytes_exchanged != strlen(answer) + 1) {
        logexit("send");
    }
}

void subscribe(int subscriber, const std::string& tag) {
    pthread_mutex_lock(&mutex);
    std::cout <<"essa tag tem tamanho: "<< tag.length()<< std::endl;

    // If client not subscribed to tag
    if (tags_to_subscribers[tag].find(subscriber) == tags_to_subscribers[tag].end()) {

        // add subscriber to tag
        tags_to_subscribers[tag].insert(subscriber);

        // add tag to this subscriber
        if(subscribers_tags.find(subscriber) == subscribers_tags.end())
            subscribers_tags.insert({{subscriber, std::set<std::string>{tag}}});
        else
            subscribers_tags[subscriber].insert(tag);

        answer_client(subscriber, "subscribed");
    } else
        answer_client(subscriber, "already subscribed");

    std::cout << "subs to tag "<< tag << ": ";
    for(auto sub : tags_to_subscribers[tag]){
        std::cout << sub << " ";
    }
    std::cout << std::endl;

    pthread_mutex_unlock(&mutex);

}

void unsubscribe(int subscriber, const std::string& tag){
    pthread_mutex_lock(&mutex);

    std::cout <<"essa tag tem tamanho: "<< tag.length()<< std::endl;

    // If client subscribes to tag, remove client from tag
    std::set<int>::iterator it;
    it = tags_to_subscribers[tag].find(subscriber);
    if(it != tags_to_subscribers[tag].end()){
        tags_to_subscribers[tag].erase(it);

        // remove tag from subscriber
        subscribers_tags[subscriber].erase(tag);

        answer_client(subscriber, "unsubscribed");
    } else{
        answer_client(subscriber, "not subscribed");
    }

    std::cout << "subs to tag "<< tag << ": ";
    for(auto sub : tags_to_subscribers[tag]){
        std::cout << sub << " ";
    }
    std::cout << std::endl;
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
void erase_client_control_data(int subscriber){
    pthread_mutex_lock(&mutex);

    // remove sub from tags
    for(auto& tag : subscribers_tags[subscriber]){
        tags_to_subscribers[tag].erase(subscriber);
    }

    // remove sub
    auto it = subscribers_tags.find(subscriber);
    subscribers_tags.erase(it);

    pthread_mutex_unlock(&mutex);

}

std::string receive_message(const ClientData *client) {
    std::string message_from_client;
    char message_part[BUFSZ];
    size_t bytes_exchanged;
    char last_char;
    std::cout << "Waiting for message..." << std::endl;
    do{
        bytes_exchanged = recv(client->socket_descriptor, message_part, BUFSZ - 1, 0);
        std::cout << "Bytes recebidos: " << bytes_exchanged << std::endl;
        if(bytes_exchanged > 500){
            std::cout << "Msg maior que 500" << std::endl;
            erase_client_control_data(client->socket_descriptor);
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

std::set<std::string> get_tags(const std::string& message){
    pthread_mutex_lock(&mutex);

    std::istringstream message_stream(message);
    std::string token;
    std::set<std::string> tags;
    while(message_stream){
        message_stream >> token;
        if(token[0] == '#'){
            tags.insert(token.substr(1));
        }
    }
    std::cout << tags.size() << std::endl;
    pthread_mutex_unlock(&mutex);
    return tags;
}


std::set<int> get_subscribers(const std::set<std::string>& tags){
    pthread_mutex_lock(&mutex);
    std::set<int> subscribers;
    for(const auto& tag : tags){
        for(auto sub : tags_to_subscribers[tag]) {
            subscribers.insert(sub);
        }
    }
    std::cout << "Current subscribers to this tags: " << subscribers.size() << std::endl;
    pthread_mutex_unlock(&mutex);
    return subscribers;
}

void distribute_messages(const std::set<int>& subscribers, const std::string& message){

    // convert string to char*
    const char *c_str_message;
    c_str_message = message.c_str();
    char c_message[BUFSZ];
    strcpy(c_message, c_str_message);

    for(auto client : subscribers){
        answer_client(client, c_message);
    }
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
                erase_client_control_data(client->socket_descriptor);
                close(client->socket_descriptor);
                pthread_exit(nullptr);
            }
        }
        std::cout << "Mensagem recebida: "<< message_from_client;

        // Check for kill
        if(message_from_client == "##kill\n"){
            exit(EXIT_SUCCESS);
        }

        // Decide action
        char first_char = message_from_client[0];
        switch (first_char) {
            case '+': {
                std::string tag = message_from_client.substr(1, message_from_client.length()-2);
                subscribe(client->socket_descriptor, tag);
                break;
            }
            case '-': {
                std::string tag = message_from_client.substr(1,message_from_client.length()-2);
                unsubscribe(client->socket_descriptor, tag);
                break;
            }
            default: {
                std::set<std::string> tags = get_tags(message_from_client);
                std::cout << "# of tags found in message: " << tags.size() << std::endl;
                if (!tags.empty()){
                    std::set<int> subscribers = get_subscribers(tags);
                    if(!subscribers.empty())
                        distribute_messages(subscribers, message_from_client);
                }

            }
        }// switch
    } // while
}// client_thread


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