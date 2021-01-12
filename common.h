// inclui esse arquivo apenas uma vez
#pragma once

#include <stdlib.h>
#include <arpa/inet.h>

void logexit(const char *msg);

void addrtostr(const struct sockaddr *addr, char *str, size_t strsize);

int client_sockaddr_init(const char *addrstr, const char *portstr,
              struct sockaddr_storage *storage);

int server_sockaddr_init(const char *proto, const char *portstr,
                         struct sockaddr_storage *storage);
