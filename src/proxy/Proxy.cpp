#include "proxy/Proxy.h"
#include <iostream>
#include <cstring>
#include <fcntl.h>

#define TAG "proxy"

pollfd Proxy::initialize_pollfd(int fd) {
    pollfd new_pollfd{};
    new_pollfd.fd = fd;
    new_pollfd.events = POLLIN | POLLOUT;
    return new_pollfd;
}

Proxy::Proxy(int port) : log(*new Logger(Constants::DEBUG, std::cout)), alive(true) {
    sockaddr_in addr{};
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    client_accepting_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_accepting_socket == -1) {
        log.err(TAG, "Failed to crate socket");
        exit(EXIT_FAILURE);
    } else {
        log.info(TAG, "Created socket for proxy server");
    }
    if (bind(client_accepting_socket, (sockaddr *) (&addr), sizeof(addr)) == -1) {
        log.err(TAG, "Failed to bind socket");
        exit(EXIT_FAILURE);
    } else {
        log.info(TAG, "Binded socket for proxy server");
    }
//    poll_fds.push_back(initialize_pollfd(client_accepting_socket));
    insert_socket(client_accepting_socket, (sockaddr *) &addr, sizeof(addr), ACCEPTING);
    casher = new Casher();
    log.deb(TAG, "Casher initialized");
}

void Proxy::start_listening_mode() {
    if (listen(client_accepting_socket, 5) == -1) {
        log.err(TAG, "Error starting listening mode");
    } else {
        log.info(TAG, "Listening mode started");
    }
}

void Proxy::try_accept_client() {
    int flags_bu = fcntl(client_accepting_socket, F_GETFL);
    if (flags_bu == -1) {
        log.err(TAG, "Failed to get listening socket flags");
    } else {
        log.deb(TAG, "Got listening socket flags");
    }
    if (fcntl(client_accepting_socket, F_SETFL, flags_bu | O_NONBLOCK) == -1) {
        log.err(TAG, "Failed to set listening socket non-block");
    } else {
        log.deb(TAG, "Listening socket set non-block");
    }

    auto addr = new sockaddr;
    socklen_t addr_len;

    int new_client = accept(client_accepting_socket, addr, &addr_len);
    if (new_client == -1) {
        if (errno != 11) {
            log.err(TAG, "Failed to accept new client " + std::to_string(errno));
        }
    } else {
        log.info(TAG, "Accepted new client on socket " + std::to_string(new_client));
        insert_socket(new_client, addr, addr_len, CLIENT);
    }

    if (fcntl(client_accepting_socket, F_SETFL, flags_bu) == -1) {
        log.err(TAG, "Failed to backup listening socket");
    } else {
        log.deb(TAG, "Listening socket backed-up successfully");
    }


}

void Proxy::launch() {
    int poll_return = 0;
    while (alive) {
        poll_return = poll(poll_fds.data(), poll_fds.size(), 0);
        if (poll_return == -1) {
            log.err(TAG, "Error in poll");
            break;
        } else {
            for (auto &poll_fd: poll_fds) {
                auto find_result = sockets.find(poll_fd.fd);
                if (find_result == sockets.end()) {
                    log.err(TAG, "Socket not found");
                    exit(EXIT_FAILURE);
                }
                Socket *cur_sock = (*find_result).second;
                if (cur_sock->type == CLIENT) {
                    if (!socketHandlers.at(poll_fd.fd)->work(poll_fd.revents)) {
                        remove_client(poll_fd.fd);
                    }
                    poll_fd.revents = 0;
                } else if (cur_sock->type == ACCEPTING) {
                    if (poll_fd.revents & POLLIN) {
                        try_accept_client();
                    }
                }
            }
        }
    }
}

void Proxy::remove_client(int socket) {
    auto it = poll_fds.begin();
    while (it->fd != socket) it++;
    poll_fds.erase(it);
    sockets.erase(socket);
    socketHandlers.erase(socket);
    close(socket);
    //TODO implement transfer of duty if the socket was SERVER
    log.info(TAG, "Client " + std::to_string(socket) + " disconnected");
}

void Proxy::insert_socket(int new_socket, const sockaddr *sockAddr, socklen_t sockLen, SocketType type) {
    auto sock = new Socket(new_socket, sockAddr, sockLen, type);
    sockets.insert(std::pair<int, Socket *>(new_socket, sock));
    if (type == CLIENT) {
        socketHandlers.insert(std::pair<int, SocketHandler *>(new_socket, new SocketHandler(new_socket, *casher)));
        log.deb(TAG, "New SocketHandler for socket " + std::to_string(new_socket) + " created");
    } else if (type == SERVER) {
//        serverHandlers.insert(std::pair<int, ServerHandler*>(new_socket, new ServerHandler(new_socket, casher)));
        log.deb(TAG, "New ServerHandler for socket " + std::to_string(new_socket) + " created");
    }
    poll_fds.push_back(initialize_pollfd(new_socket));
    log.deb(TAG, "Pushed " + std::to_string(new_socket) + " to pollfd vector");
}

