set(HEADERS_LIST
        "${proxy_syn_SOURCE_DIR}/include/proxy/Proxy.h"
        "${proxy_syn_SOURCE_DIR}/include/casher/Casher.h"
        "${proxy_syn_SOURCE_DIR}/include/logger.hpp"
        "${proxy_syn_SOURCE_DIR}/include/http_parser/http_parser.h"
        logger.cpp
        ../include/Constants.h
        ../include/net/Socket.h
        net/Socket.cpp
        ../include/proxy/SocketHandler.h)

add_library(proxy_lib
        proxy/SocketHandler.cpp
        proxy/Proxy.cpp
        http_parser/http_parser.c
        ${HEADERS_LIST} casher/Casher.cpp)

find_package(Threads REQUIRED)
if (THREAD_HAVE_PTHREAD_ARG)
    target_compile_options(proxy_lib PUBLIC "-pthread")
endif ()


if (CMAKE_THREAD_LIBS_INIT)
    target_link_libraries(proxy_lib "${CMAKE_THREAD_LIBS_INIT}")
endif ()

if (${CMAKE_VERSION} VERSION_GREATER "3.1")
    set(THREADS_PREFER_PTHREAD_FLAG ON)
endif ()


target_include_directories(proxy_lib PUBLIC ../include)