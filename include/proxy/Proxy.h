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

    static pollfd initializePollfd(int fd, SocketType type);

    void insertSocket(int new_socket, const sockaddr *sockAddr, socklen_t sockLen, SocketType type);

    void removeClient(int socket);

    void removeServer(int server_sock);

    SocketHandler *findByServerSocket(int server_socket);

    bool tryChooseDeputy(int socket);

    void* SIGINTHandler(int sig);

public:
    void startListeningMode();

    bool tryAcceptClient();

    explicit Proxy(int port);

    virtual ~Proxy();

    void launch();

};