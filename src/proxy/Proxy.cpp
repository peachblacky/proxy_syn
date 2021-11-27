#include "proxy/Proxy.h"
#include <cstring>
#include <fcntl.h>

#define TAG "proxy"

pollfd Proxy::initialize_pollfd(int fd, SocketType type) {
    pollfd new_pollfd{};
    new_pollfd.fd = fd;
    if (type == ACCEPTING) {
        new_pollfd.events = POLLIN;
    } else {
        new_pollfd.events = POLLIN | POLLOUT;
    }
    return new_pollfd;
}

Proxy::Proxy(int port) : log(new Logger(Constants::DEBUG, std::cerr)), alive(true) {
    sockaddr_in addr{};
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    client_accepting_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_accepting_socket == -1) {
        log->err(TAG, "Failed to crate socket");
        exit(EXIT_FAILURE);
    } else {
        log->info(TAG, "Created socket for proxy server");
    }
    if (bind(client_accepting_socket, (sockaddr *) (&addr), sizeof(addr)) == -1) {
        log->err(TAG, "Failed to bind socket");
        exit(EXIT_FAILURE);
    } else {
        log->info(TAG, "Binded socket for proxy server");
    }
//    poll_fds.push_back(initialize_pollfd(client_accepting_socket));
    insert_socket(client_accepting_socket, (sockaddr *) &addr, sizeof(addr), ACCEPTING);
    cacher = new Cacher();
    log->deb(TAG, "Casher initialized");
}

void Proxy::start_listening_mode() {
    if (listen(client_accepting_socket, 5) == -1) {
        log->err(TAG, "Error starting listening mode");
    } else {
        log->info(TAG, "Listening mode started");
    }
}

bool Proxy::try_accept_client() {
    int flags_bu = fcntl(client_accepting_socket, F_GETFL);
    if (flags_bu == -1) {
        log->err(TAG, "Failed to get listening socket flags");
        exit(EXIT_FAILURE);
    } else {
        log->deb(TAG, "Got listening socket flags");
    }
    if (fcntl(client_accepting_socket, F_SETFL, flags_bu | O_NONBLOCK) == -1) {
        log->err(TAG, "Failed to set listening socket non-block");
        exit(EXIT_FAILURE);
    } else {
        log->deb(TAG, "Listening socket set non-block");
    }

    auto addr = new sockaddr;
    socklen_t addr_len;

    int new_client = accept(client_accepting_socket, addr, &addr_len);
    if (new_client == -1) {
        log->err(TAG, "Failed to accept new client " + std::to_string(errno));
        return false;
    } else {
        log->info(TAG, "Accepted new client on socket " + std::to_string(new_client));
        insert_socket(new_client, addr, addr_len, CLIENT);
    }

    if (fcntl(client_accepting_socket, F_SETFL, flags_bu) == -1) {
        log->err(TAG, "Failed to backup listening socket");
        exit(EXIT_FAILURE);
    } else {
        log->deb(TAG, "Listening socket backed-up successfully");
    }
    return true;


}

SocketHandler *Proxy::find_by_server_socket(int server_socket) {
    auto it = socketHandlers.begin();
    while (it != socketHandlers.end()) {
        if (it->second->getServerSocket() == server_socket) {
//            log->deb(TAG, "Found handler for server " + std::to_string(server_socket));
            return it->second;
        }
        it++;
    }
    log->err(TAG, "Not found handler for server " + std::to_string(server_socket));
    return nullptr;
}

void Proxy::launch() {
    int poll_return = 0;
    while (alive) {
        //TODO mb make a separate function for poll-handling
        poll_return = poll(poll_fds.data(), poll_fds.size(), -1);
        if (poll_return == -1) {
            break;
        } else {
            for (auto &poll_fd: poll_fds) {
                auto find_result = sockets.find(poll_fd.fd);
                if (find_result == sockets.end()) {
                    log->err(TAG, "Socket " + std::to_string(poll_fd.fd) + " not found");
                    exit(EXIT_FAILURE);
                }
                Socket *cur_sock = (*find_result).second;
                if (cur_sock->type == CLIENT) {
                    auto foundHandler = socketHandlers.at(poll_fd.fd);
                    if (!foundHandler->work(poll_fd.revents, CLIENT)) {
                        remove_client(poll_fd.fd);
                        break;
                    }
                    if(foundHandler->isConnectedToServerThisTurn()) {
                        break;
                    }
                    poll_fd.revents = 0;
                } else if (cur_sock->type == ACCEPTING) {
                    if (poll_fd.revents & POLLIN) {
                        if(try_accept_client()) {
                            break;
                        }
                    }
                } else if (cur_sock->type == SERVER) {
                    SocketHandler *found_handler = find_by_server_socket(poll_fd.fd);
                    if (found_handler == nullptr) {
                        remove_server(poll_fd.fd);
                        break;
                    } else {
                        if (!found_handler->work(poll_fd.revents, SERVER)) {
                            //TODO move it to separate function
                            remove_server(poll_fd.fd);
//                            remove_client(found_handler->getClientSocket());
                            break;
                        }
                    }
                }
            }
        }
    }
}

void Proxy::remove_client(int socket) {
//    log->deb(TAG, "Deleting client");

    /* Пытаемся выбрать deputy, и если не получается
     * - удаляем хэндлер вместе со всеми сокетами,
     * иначе наследуем обязаности серверного хэндлера*/
    auto it = poll_fds.begin();
    while (it != poll_fds.end()) {
        if(it->fd == socket) {
            break;
        }
        it++;
    }

//    if() {
//      TODO
//    }

    if(socketHandlers.at(socket)->getType() != CASH) {
        remove_server(socketHandlers.at(socket)->getServerSocket());
    }



    poll_fds.erase(it);
    sockets.erase(socket);
    delete socketHandlers.at(socket);
    socketHandlers.erase(socket);
    close(socket);
    log->err(TAG, "Client " + std::to_string(socket) + " disconnected");
    log->deb(TAG, "Open sockets left " + std::to_string(sockets.size()));
}

bool Proxy::try_choose_deputy() {
    return false;
}

void Proxy::remove_server(int server_sock) {
//    log->deb(TAG, "Disconnecting server");
    auto it = poll_fds.begin();
    while (it != poll_fds.end()) {
        if(it->fd == server_sock) {
            break;
        }
        it++;
    }
    if(it == poll_fds.end()) {
        return;
    }

    poll_fds.erase(it);
    sockets.erase(server_sock);
    close(server_sock);
    log->err(TAG, "Server " + std::to_string(server_sock) + " disconnected");
}

void Proxy::insert_socket(int new_socket, const sockaddr *sockAddr, socklen_t sockLen, SocketType type) {
    poll_fds.push_back(initialize_pollfd(new_socket, type));
//    log->deb(TAG, "Pushed " + std::to_string(new_socket) + " to pollfd vector");
    auto sock = new Socket(new_socket, sockAddr, sockLen, type);
    sockets.insert(std::pair<int, Socket *>(new_socket, sock));
    socketHandlers.insert(
            std::pair<int, SocketHandler *>(new_socket, new SocketHandler(new_socket, cacher, poll_fds, sockets)));
    log->deb(TAG, "New SocketHandler for socket " + std::to_string(new_socket) + " created");
}

Proxy::~Proxy() {
    delete cacher;
    for (auto &a: sockets) {
        delete a.second;
    }
    for (auto &a: socketHandlers) {
        delete a.second;
    }
}



