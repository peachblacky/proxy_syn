//
// Created by Sibdroid on 11/8/2021.
//

#include <net/Socket.h>

Socket::Socket(int sockfd, const sockaddr *sockAddr, socklen_t sockLen, const SocketType type) : sockfd(sockfd),
                                                                                                 sockAddr(sockAddr),
                                                                                                 sockLen(sockLen),
                                                                                                 type(type) {
}
