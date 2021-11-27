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
    Logger *log;
    Cacher *cacher;
    std::vector<pollfd> poll_fds;
    std::map<int, SocketHandler *> socketHandlers;
    std::map<int, Socket *> sockets;
    bool alive;

    int client_accepting_socket;

    static pollfd initialize_pollfd(int fd, SocketType type);

    void insert_socket(int new_socket, const sockaddr *sockAddr, socklen_t sockLen, SocketType type);

    void remove_client(int socket);

    void remove_server(int server_sock);

    SocketHandler* find_by_server_socket(int server_socket);

    bool try_choose_deputy();

public:
    void start_listening_mode();

    bool try_accept_client();

    explicit Proxy(int port);

    virtual ~Proxy();

    void launch();

};