#pragma once

#include <casher/Casher.h>
#include "logger.hpp"
#include "http_parser/http_parser.h"

enum HandlerType {CASH, LOAD, STANDBY};

//typedef struct {
//
//} request_data;

class SocketHandler {
private:
    HandlerType type;
    int client_socket;

    //buffer for reading from socket
    static const ssize_t req_buf_capacity = BUFSIZ;
    ssize_t req_buf_len;
    char* req_buf;

    //buffer for parsing headers
    std::string cur_header_field;
    std::string cur_header_value;

    //Request itself
    std::string url;
    std::map<std::string, std::string> headers;

    //Casher and logger
    Casher &casher;
    Logger log;

    //State variables
    bool req_in_process;
    bool req_ready;
    bool last_was_value;

    //Parser
    http_parser* parser;
    http_parser_settings* settings;

    static int url_callback(http_parser* parser, const char *at, size_t length);
    static int header_field_callback(http_parser* parser, const char *at, size_t length);
    static int header_value_callback(http_parser* parser, const char *at, size_t length);
    static int headers_complete_callback(http_parser* parser);

    void init_parser();
    bool receive_request_data();

public:
    bool work(short revents);

    int write_data();

    SocketHandler(int sockfd, Casher &casher);

};
