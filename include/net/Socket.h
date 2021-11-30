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

    const int sockfd = -1;
    const sockaddr *sockAddr;
    const socklen_t sockLen = 0;
    const SocketType type = SocketType ::CLIENT;
};

#endif //PROXY_SYN_SOCKET_H
