#pragma once

#include <casher/Casher.h>
#include "logger.hpp"
#include "http_parser/http_parser.h"
#include <vector>
#include <net/Socket.h>

enum HandlerType {
    CASH,
    LOAD_CASHING,
    LOAD_TRANSIENT,
    STANDBY
};

enum ParserMode {
    REQUEST, RESPONSE
};

//typedef struct {
//
//} request_data;

class SocketHandler {
private:
    HandlerType type;
    int client_socket;
    int server_socket;
    std::vector<pollfd> &poll_fds_ref;
    std::map<int, Socket *> &sockets_ref;

    //Request itself
    static const ssize_t req_buff_capacity = BUFSIZ;
//    char *request_buff;
    std::string request_full;
//    ssize_t req_buff_len;
    ssize_t req_sent_bytes;

    //Response itself
    static const ssize_t resp_buff_capacity = BUFSIZ;
//    char *response_buff;
    std::string response_full;
//    ssize_t resp_buf_len;
    ssize_t resp_sent_bytes;

    //buffer for parsing req_headers
    std::string cur_header_field;
    std::string cur_header_value;

    //Parsed request of client
    std::string req_url;
    std::map<std::string, std::string> req_headers;
    std::string req_body;


    //Parsed response of server
    std::map<std::string, std::string> resp_headers;
    std::string resp_body;

    //Casher and logger
    Cacher &cacher;
    Logger log;

    //Parser states
    bool last_was_value;

    //Request states
    bool req_in_process;
    bool req_ready;
    bool req_sent;

    //Response states
    bool resp_ready;
    bool resp_sent;

    //Other states
    bool connected_to_server_this_turn;

    //Parser
    http_parser *resp_parser;
    http_parser_settings *resp_settings;
    http_parser *req_parser;
    http_parser_settings *req_settings;
    http_parser_url *parsed_url;
    ParserMode parser_mode;

    static int url_callback(http_parser *parser, const char *at, size_t length);

    static int header_field_callback(http_parser *parser, const char *at, size_t length);

    static int header_value_callback(http_parser *parser, const char *at, size_t length);

    static int message_complete_callback(http_parser *parser);

    static int body_callback(http_parser *parser, const char *at, size_t length);

    void init_request_parser();

    void init_response_parser();

    bool receive_and_parse_request();

    bool receive_and_parse_response();

    bool acquire_handler_type();

    bool send_request();

    bool connect_to_server();

    bool send_response();

    bool retrieve_data_from_cache();

    bool transfer_response_from_cash();

    bool receive_response();

public:
    bool work(short revents, SocketType sock_type);

    bool work_server(short revents);

    int getServerSocket() const;

    HandlerType getType() const;

    int getClientSocket() const;

    SocketHandler(int sockfd, Cacher &casher, std::vector<pollfd> &pfds_ref, std::map<int, Socket *> &sockets_ref);

    virtual ~SocketHandler();

    bool isConnectedToServerThisTurn() const;
};
