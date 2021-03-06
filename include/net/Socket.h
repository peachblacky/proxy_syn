//
// Created by Sibdroid on 11/8/2021.
//
#pragma once
#ifndef PROXY_SYN_SOCKET_H
#define PROXY_SYN_SOCKET_H

#include "netdb.h"

enum SocketType {
    CLIENT, SERVER, ACCEPTING
};

class Socket {
public:
    Socket(int sockfd, const sockaddr *sockAddr, socklen_t sockLen, SocketType type);

    virtual ~Socket() = default;

    const int sockfd;
    const sockaddr *sockAddr;
    const socklen_t sockLen;
    const SocketType type;
};

#endif //PROXY_SYN_SOCKET_H
