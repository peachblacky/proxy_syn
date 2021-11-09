#pragma once

#include "netdb.h"
#include "unistd.h"
#include <poll.h>
#include <vector>
#include <map>
#include <cstdlib>
#include "../casher/Casher.h"
#include "./SocketHandler.h"
#include "logger.hpp"
#include "Constants.h"
#include "net/Socket.h"


class Proxy {
private:
    Logger log;
    Casher *casher;
    std::vector<pollfd> poll_fds;
    std::map<int, SocketHandler *> socketHandlers;
    std::map<int, Socket *> sockets;
    bool alive;

    int client_accepting_socket;

    static pollfd initialize_pollfd(int fd);

    void insert_socket(int new_socket, const sockaddr *sockAddr, socklen_t sockLen, SocketType type);

    void remove_client(int socket);

public:
    void start_listening_mode();

    void try_accept_client();

    explicit Proxy(int port);

    void launch();
};