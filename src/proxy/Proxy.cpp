#include "proxy/Proxy.h"
#include <cstring>
#include <fcntl.h>

#define TAG "proxy"


pollfd Proxy::initializePollfd(int fd, SocketType type) {
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
    insertSocket(client_accepting_socket, (sockaddr *) &addr, sizeof(addr), ACCEPTING);
    cacher = new Cacher();
    log->deb(TAG, "Casher initialized");
}

void Proxy::startListeningMode() {
    if (listen(client_accepting_socket, 5) == -1) {
        log->err(TAG, "Error starting listening mode");
    } else {
        log->info(TAG, "Listening mode started");
    }
}

bool Proxy::tryAcceptClient() {
    auto addr = new sockaddr;
    socklen_t addr_len;

    int new_client = accept(client_accepting_socket, addr, &addr_len);
    if (new_client == -1) {
        log->err(TAG, "Failed to accept new client " + std::to_string(errno));
        return false;
    } else {
        log->info(TAG, "Accepted new client on socket " + std::to_string(new_client));
        insertSocket(new_client, addr, addr_len, CLIENT);
    }
    return true;


}

SocketHandler *Proxy::findByServerSocket(int server_socket) {
    auto it = socketHandlers.begin();
    while (it != socketHandlers.end()) {
        if (it->second->getServerSocket() == server_socket) {
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
                        removeClient(poll_fd.fd);
                        break;
                    }
                    if (foundHandler->isConnectedToServerThisTurn()) {
                        break;
                    }
                    poll_fd.revents = 0;
                } else if (cur_sock->type == ACCEPTING) {
                    if(poll_fd.revents & (POLLERR | POLLHUP)) {
                        break;
                    }
                    if (poll_fd.revents & POLLIN) {
                        if (tryAcceptClient()) {
                            break;
                        }
                    }
                } else if (cur_sock->type == SERVER) {
                    SocketHandler *found_handler = findByServerSocket(poll_fd.fd);
                    if (found_handler == nullptr) {
                        removeServer(poll_fd.fd);
                        break;
                    } else {
                        if (!found_handler->work(poll_fd.revents, SERVER)) {

//                            removeServer(poll_fd.fd);
                            removeClient(found_handler->getClientSocket());
                            break;
                        }
                    }
                }
                poll_fd.revents = 0;
            }
        }
    }
}

void Proxy::removeClient(int socket) {
    log->deb(TAG, "Deleting client");

    /* Пытаемся выбрать deputy, и если не получается
     * - удаляем хэндлер вместе со всеми сокетами,
     * иначе наследуем обязаности серверного хэндлера*/
    if (socketHandlers.at(socket)->getType() != CASH) {
        if(!tryChooseDeputy(socket)) {
            removeServer(socketHandlers.at(socket)->getServerSocket());
        } else {
            log->info(TAG, "Deputy for server "
            + std::to_string(socketHandlers.at(socket)->getServerSocket())
            + " successfully chosen");
        }
    }

    auto it = poll_fds.begin();
    while (it != poll_fds.end()) {
        if (it->fd == socket) {
            break;
        }
        it++;
    }

    poll_fds.erase(it);
    sockets.erase(socket);
    delete socketHandlers.at(socket);
    socketHandlers.erase(socket);
    close(socket);
    log->err(TAG, "Client " + std::to_string(socket) + " disconnected");
    log->deb(TAG, "Open sockets left " + std::to_string(sockets.size()));
}

bool Proxy::tryChooseDeputy(int socket) {
    auto it = socketHandlers.begin();
    size_t max_cache_position = 0;
    auto masterHandler = socketHandlers.at(socket);
    if (masterHandler->getServerSocket() == -1) {
        log->err(TAG, "Not possible to choose deputy for client reading from cash");
        return false;
    }
    SocketHandler *deputy = nullptr;
//    log->deb(TAG, "Choosing dep");
    while (it != socketHandlers.end()) {
        if (it->second->getType() == CASH) {
            if (it->second->getReqUrl() == masterHandler->getReqUrl()
                && it->second->getRespCachePosition() > max_cache_position) {
                deputy = it->second;
            }
        }
        it++;
    }
    if (deputy == nullptr) {
        log->deb(TAG, "No deputy for server socket " + masterHandler->getServerSocket());
        return false;
    }
    deputy->becomeHeir(masterHandler);

    return true;
}

void Proxy::removeServer(int server_sock) {
    auto it = poll_fds.begin();
    while (it != poll_fds.end()) {
        if (it->fd == server_sock) {
            break;
        }
        it++;
    }
    if (it == poll_fds.end()) {
        return;
    }

    poll_fds.erase(it);
    sockets.erase(server_sock);
    close(server_sock);
    log->err(TAG, "Server " + std::to_string(server_sock) + " disconnected");
}

void Proxy::insertSocket(int new_socket, const sockaddr *sockAddr, socklen_t sockLen, SocketType type) {
    poll_fds.push_back(initializePollfd(new_socket, type));
    auto sock = new Socket(new_socket, sockAddr, sockLen, type);
    sockets.insert(std::pair<int, Socket *>(new_socket, sock));
    socketHandlers.insert(
            std::pair<int, SocketHandler *>(new_socket, new SocketHandler(new_socket, cacher, poll_fds, sockets)));
    log->deb(TAG, "New SocketHandler for socket " + std::to_string(new_socket) + " created");
}

Proxy::~Proxy() {
    delete log;
    delete cacher;
    for (auto &a: sockets) {
        delete a.second;
    }
    for (auto &a: socketHandlers) {
        delete a.second;
    }
    close(client_accepting_socket);
}



