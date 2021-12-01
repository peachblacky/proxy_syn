#! /bin/sh
g++ -lsocket -lnsl utils/Application.cpp src/http_parser/http_parser.c src/logger.cpp src/net/Socket.cpp src/proxy/SocketHandler.cpp src/proxy/Proxy.cpp src/casher/Casher.cpp -I ./include/
./a.out 1111