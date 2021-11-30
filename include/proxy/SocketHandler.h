#pragma once

#include <casher/Casher.h>
#include "logger.hpp"
#include "http_parser/http_parser.h"
#include <vector>
#include <net/Socket.h>
#include <strings.h>
#include <signal.h>

enum HandlerType {
    CASH,
    LOAD_CASHING,
    LOAD_TRANSIENT,
    STANDBY,
    HEIR // temporal role for heir of ServerHandler until he reaches the front end of cache
};

enum ParserMode {
    REQUEST, RESPONSE
};

class SocketHandler {
private:
    HandlerType type;
    int client_socket;
    int server_socket;
    std::vector<pollfd> &poll_fds_ref;
    std::map<int, Socket *> &sockets_ref;
    SocketHandler* ancestor;

    //Request itself
    static const ssize_t req_buff_capacity;
    std::string request_full;
    ssize_t req_sent_bytes;

    //Response itself
    static const ssize_t resp_buff_capacity;
    size_t resp_cache_position;

    //buffer for parsing req_headers
    std::string cur_header_field;
    std::string cur_header_value;

    //Parsed request of client
    std::string req_url;
    std::map<std::string, std::string> req_headers;
    std::string req_body;


    //Parsed response of server
    std::map<std::string, std::string> resp_headers;

    //Casher and logger
    Cacher *cacher;
    Logger *log;

    //Parser states
    bool last_was_value;

    //Request states
    bool req_ready;
    bool req_sent;

    //Response states
    bool resp_ready;
    bool resp_sent;

    //Other states
    bool connected_to_server_this_turn;
    bool has_heir;

    //Parser
    http_parser *resp_parser{};
    http_parser_settings *resp_settings{};
    http_parser *req_parser{};
    http_parser_settings *req_settings{};
    http_parser_url *parsed_url{};
    ParserMode parser_mode;

    static int urlCallback(http_parser *parser, const char *at, size_t length);

    static int headerFieldCallback(http_parser *parser, const char *at, size_t length);

    static int headerValueCallback(http_parser *parser, const char *at, size_t length);

    static int messageCompleteCallback(http_parser *parser);

    void initRequestParser();

    void initResponseParser();

    bool receiveAndParseRequest();

    bool receiveAndParseResponse();

    bool acquireHandlerType();

    bool sendRequest();

    bool connectToServer();

    bool sendResponseChunk(char *buffer, size_t len);

    bool sendChunkFromCache();

    void becomeMaster();

public:
    SocketHandler(int sockfd, Cacher *casher, std::vector<pollfd> &pfds_ref, std::map<int, Socket *> &sockets_ref);

    bool work(short revents, SocketType sock_type);

    int getServerSocket() const;

    HandlerType getType() const;

    size_t getRespCachePosition() const;

    const std::string &getReqUrl() const;

    void becomeHeir(SocketHandler* newAncestor);

    virtual ~SocketHandler();

    bool isConnectedToServerThisTurn() const;

    void setHasHeir(bool hasHeir);

    Cacher *getCacher() const;
};
