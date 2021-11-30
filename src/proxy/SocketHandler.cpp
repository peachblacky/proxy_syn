#include <Constants.h>
#include <sys/poll.h>
#include "proxy/SocketHandler.h"
#include <unistd.h>

#define TAG "socket_handler"

SocketHandler::SocketHandler(int sockfd, Cacher *casher, std::vector<pollfd> &pfds_ref,
                             std::map<int, Socket *> &sockets_ref) : cacher(casher),
                                                                     client_socket(sockfd),
                                                                     server_socket(-1),
                                                                     parser_mode(REQUEST),
                                                                     log(new Logger(
                                                                             Constants::DEBUG,
                                                                             std::cerr)),
                                                                     type(STANDBY),
                                                                     req_ready(false),
                                                                     req_sent(false),
                                                                     resp_sent(false),
                                                                     resp_ready(false),
                                                                     resp_buff_capacity(BUFSIZ),
                                                                     req_buff_capacity(BUFSIZ),
                                                                     resp_cache_position(0),
                                                                     poll_fds_ref(pfds_ref),
                                                                     sockets_ref(sockets_ref),
                                                                     req_sent_bytes(0) {
    initResponseParser();
    initRequestParser();
    sigset(SIGPIPE, SIG_IGN);
}

void SocketHandler::initRequestParser() {
    req_settings = new http_parser_settings;
    http_parser_settings_init(req_settings);
    req_settings->on_url = urlCallback;
    req_settings->on_header_field = headerFieldCallback;
    req_settings->on_header_value = headerValueCallback;
    req_settings->on_message_complete = messageCompleteCallback;
    cur_header_field.clear();
    cur_header_value.clear();
    last_was_value = false;

    req_parser = static_cast<http_parser *>(malloc(sizeof(http_parser)));
    http_parser_init(req_parser, HTTP_REQUEST);
}

void SocketHandler::initResponseParser() {
    resp_settings = new http_parser_settings;
    http_parser_settings_init(resp_settings);
    resp_settings->on_message_complete = messageCompleteCallback;

    resp_parser = static_cast<http_parser *>(malloc(sizeof(http_parser)));
    http_parser_init(resp_parser, HTTP_RESPONSE);
}

int SocketHandler::urlCallback(http_parser *parser, const char *at, size_t length) {
    auto sh = static_cast<SocketHandler *>(parser->data);
    sh->req_url.append(at, length);
    return 0;
}

int SocketHandler::headerFieldCallback(http_parser *parser, const char *at, size_t length) {
    auto sh = static_cast<SocketHandler *>(parser->data);
    if (sh->last_was_value) {
        if (sh->parser_mode == REQUEST) {
            sh->req_headers.insert(std::pair<std::string, std::string>(sh->cur_header_field, sh->cur_header_value));
        } else if (sh->parser_mode == RESPONSE) {
            sh->resp_headers.insert(std::pair<std::string, std::string>(sh->cur_header_field, sh->cur_header_value));
        }

        sh->cur_header_field.clear();
        sh->cur_header_value.clear();

        sh->cur_header_field.append(at, length);
    } else {
        if (!sh->cur_header_value.empty()) {
            return 1;
        }

        sh->cur_header_field.append(at, length);
    }
    sh->last_was_value = false;
    return 0;
}

int SocketHandler::headerValueCallback(http_parser *parser, const char *at, size_t length) {
    auto sh = static_cast<SocketHandler *>(parser->data);
    if (!sh->last_was_value) {
        if (!sh->cur_header_value.empty()) {
            return 1;
        }

        sh->cur_header_value.append(at, length);
    } else {

        sh->cur_header_field.append(at, length);
    }
    sh->last_was_value = true;
    return 0;
}

int SocketHandler::messageCompleteCallback(http_parser *parser) {
    auto sh = static_cast<SocketHandler *>(parser->data);
    if (sh->parser_mode == REQUEST) {
        sh->req_ready = true;
    } else if (sh->parser_mode == RESPONSE) {
        sh->getCacher()->setFullyLoaded(sh->req_url);
        sh->resp_ready = true;
    }

    return 0;
}

bool SocketHandler::receiveAndParseRequest() {
    char request_buff[req_buff_capacity]{};

    log->deb(TAG, "Receiving request from " + std::to_string(client_socket));
    ssize_t recved = recv(client_socket, request_buff, req_buff_capacity, 0);
    log->deb(TAG, "Recved " + std::to_string(recved));
    if (recved < 0) {
        log->err(TAG, "Error receiving request from client " + std::to_string(client_socket));
        return false;
    } else if (recved == 0) {
        return false;
    }


    req_parser->data = this;


    size_t nparsed = http_parser_execute(req_parser, req_settings, request_buff, recved);
    if (nparsed != recved) {
        log->err(TAG, "Error parsing request from " + std::to_string(client_socket));
        return false;
    }

    request_full.append(request_buff, recved);

    parsed_url = new http_parser_url;
    http_parser_url_init(parsed_url);
    log->deb(TAG, "Parsing req_url : " + req_url);
    if (http_parser_parse_url(req_url.data(), req_url.length(), 0, parsed_url)) {
        log->err(TAG, "Error parsing req_url in request from " + std::to_string(client_socket));
        return false;
    }
    log->deb(TAG, "Parsed req_url : " + req_url);


    return true;
}

bool SocketHandler::receiveAndParseResponse() {
    char response_buff[resp_buff_capacity];

    log->deb(TAG, "Receiving resposne from " + std::to_string(server_socket));
    ssize_t recved = recv(server_socket, response_buff, resp_buff_capacity, 0);
    if (recved < 0) {
        log->err(TAG, "Error receiving response from client " + std::to_string(server_socket));
        return false;
    } else if (recved == 0) {
        log->deb(TAG, "Response fully received");
        resp_sent = true;
        cacher->setFullyLoaded(req_url);
        return false;
    }

    if (type == LOAD_CASHING) {
        log->deb(TAG, "Appending cache");
        auto cache_return = cacher->appendCache(req_url, response_buff, recved);
        log->deb(TAG, "Written new data in cache");
        if (cache_return == CacheReturn::NotEnoughSpace) {
            log->err(TAG, "Not enough space in cache");
            type = LOAD_TRANSIENT;
        } else if (cache_return == CacheReturn::PageNotFound) {
            return false;
        }
    }
    resp_parser->data = this;

    size_t nparsed = http_parser_execute(resp_parser, resp_settings, response_buff, recved);
    if (nparsed != recved) {
        log->err(TAG, "Error parsing response from " + std::to_string(server_socket) + " , cause only " +
                      std::to_string(nparsed) + " was parsed.\n" + "Errno is " +
                      std::to_string(resp_parser->http_errno));
        return false;
    }

    sendResponseChunk(response_buff, recved);
    return true;
}

bool SocketHandler::acquireHandlerType() {
    if (req_url.empty()) {
        log->err(TAG, "Error acquiring handler type");
        return false;
    }
    if (cacher->isCached(req_url)) {
        type = CASH;
    } else {
        type = LOAD_CASHING;
    }
    return true;
}

bool SocketHandler::connectToServer() {
    addrinfo *ailist;
    addrinfo hint{};
    bzero(&hint, sizeof(hint));
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_family = PF_UNSPEC;

    std::string url_host;
    url_host.append(req_url, parsed_url->field_data[1].off, parsed_url->field_data[1].len);


    log->deb(TAG, "Trying getting addr of server on req_url " + url_host);
    if (getaddrinfo(url_host.c_str(), "80", &hint, &ailist)) {
        log->err(TAG, "Error getting server address data " + std::to_string(errno));
        return false;
    }
    log->deb(TAG, "Successfully got addrs of server");

    for (auto aip = ailist; aip != nullptr; aip = aip->ai_next) {
        int sock;

        if ((sock = socket(aip->ai_family, aip->ai_socktype, aip->ai_protocol)) < 0) {
            log->err(TAG, "Failed to create socket for server");
            return false;
        }
        if (connect(sock, aip->ai_addr, aip->ai_addrlen) == 0) {
            log->info(TAG,
                      "Successfully connected to req_url " + req_url + " on client " + std::to_string(client_socket));
            server_socket = sock;
            pollfd new_pollfd{};
            new_pollfd.fd = sock;
            new_pollfd.events = POLLIN | POLLOUT;
            poll_fds_ref.push_back(new_pollfd);
            log->deb(TAG, "Pushed new server " + std::to_string(new_pollfd.fd) + " to pollfd vector");
            auto socket_wrap = new Socket(sock, aip->ai_addr, aip->ai_addrlen, SERVER);
            sockets_ref.insert(std::pair<int, Socket *>(sock, socket_wrap));
            connected_to_server_this_turn = true;
            return true;
        }
        close(sock);
    }
    log->err(TAG, "Couldn't connect to server by req_url " + req_url + " on socket " + std::to_string(client_socket));
    return false;
}

bool SocketHandler::sendRequest() {
    if (!connectToServer()) {
        return false;
    }
    log->deb(TAG,
              "Sending request from " + std::to_string(client_socket) + " to server " + std::to_string(server_socket));
    if (send(server_socket, request_full.c_str(), request_full.size(), 0) == -1) {
        log->err(TAG, "Error sending request from " + std::to_string(client_socket) + " to server " +
                      std::to_string(server_socket));
        return false;
    }
    log->info(TAG, "Request sent to " + std::to_string(server_socket));
    req_sent = true;
    return true;
}

bool SocketHandler::sendResponseChunk(char *buffer, size_t len) {
    if (type == LOAD_TRANSIENT || type == LOAD_CASHING) {
        log->deb(TAG,
                  "Sending response chunk from "
                  + std::to_string(server_socket)
                  + " to client "
                  + std::to_string(client_socket));
    } else {
        log->deb(TAG,
                  "Sending response chunk from cache to client "
                  + std::to_string(client_socket));
    }
    ssize_t resp_sent_bytes = 0;
    unsigned long left_to_send = len;
    unsigned long amount_to_send = 0;
    while (left_to_send > 0) {
        amount_to_send = left_to_send > resp_buff_capacity ?
                         resp_buff_capacity * (left_to_send / resp_buff_capacity) : left_to_send;
        ssize_t sent = send(client_socket, buffer + resp_sent_bytes, amount_to_send, 0);
        if (sent == -1) {
            log->err(TAG, "Error sending data from " + std::to_string(client_socket) + " to server " +
                          std::to_string(server_socket));
            log->deb(TAG, "errno is " + std::to_string(errno));
            return false;
        }
        resp_sent_bytes += sent;
        left_to_send -= sent;
    }
    if (type == CASH) {
        if (len < CACHE_CHUNK_SIZE && resp_ready) {
            log->deb(TAG, "Resp fully sent cash");
            resp_sent = true;
        }
    } else {
        if (resp_ready) {
            log->deb(TAG, "Resp fully sent load");
            resp_sent = true;
        }
    }
    return true;
}

bool SocketHandler::sendChunkFromCache() {
    char chunk_buf[Cacher::getChunkSize()];
    size_t chunk_len = 0;
    auto ret = cacher->acquireChunk(chunk_buf, chunk_len, req_url, resp_cache_position);
    if (ret != CacheReturn::OK) {
        return false;
    }
    if (chunk_len == 0) {
        if (cacher->isFullyLoaded(req_url)) {
            log->deb(TAG, "Client " + std::to_string(client_socket) + " has received all data from cache");
            resp_sent = true;
        } else {
            if (type == HEIR) {
                log->info(TAG, "Client handler "
                               + std::to_string(client_socket)
                               + " has reached cash end, becoming the master of server " +
                               std::to_string(server_socket));
                becomeMaster();
            } else {
                log->deb(TAG, "Client " + std::to_string(client_socket) + " is waiting for more data from cache");
            }
        }
        return true;
    }
    resp_cache_position += chunk_len;
    return sendResponseChunk(chunk_buf, chunk_len);
}

bool SocketHandler::work(short revents, SocketType sock_type) {
    connected_to_server_this_turn = false;
    if (resp_sent && req_sent) {
        return false;
    }

    if (revents & (POLLHUP | POLLERR)) {
        return false;
    }
    if (revents & POLLIN) {
        if (sock_type == CLIENT && !req_ready) {
            parser_mode = REQUEST;
            if (!receiveAndParseRequest()) {
                return false;
            }
        } else if (sock_type == SERVER && !resp_ready) {
            if (type == CASH) {
                log->err(TAG, "No server socket on cash handler");
                return false;
            } else if (type == HEIR) {
                return true;
            }
            parser_mode = RESPONSE;
            log->deb(TAG, "Receiving response");
            if (!receiveAndParseResponse()) {
                return false;
            }
        }
    }

    if (req_ready && !req_sent) {
        if (type == STANDBY) {
            if (!acquireHandlerType()) {
                return false;
            }
        }
        if (type == LOAD_CASHING) {
            if (!sendRequest()) {
                return false;
            }
        } else if (type == CASH) {
            req_sent = true;
        }
    }

    if (type == CASH || type == HEIR && (revents & POLLOUT) && !resp_sent) {
        return sendChunkFromCache();
    }

    return true;
}

void SocketHandler::becomeHeir(SocketHandler *newAncestor) {
    type = HEIR;
    ancestor = newAncestor;
    server_socket = newAncestor->server_socket;
    req_url = newAncestor->req_url;
    resp_parser = newAncestor->resp_parser;
    newAncestor->setHasHeir(true);
}

void SocketHandler::becomeMaster() {
    type = LOAD_CASHING;
    req_ready = true;
}

SocketHandler::~SocketHandler() {
    log->err(TAG, "Handler for " + std::to_string(client_socket) + " terminates");
    close(client_socket);
//    close(server_socket);
    if (!has_heir) {
        delete resp_parser;
        delete parsed_url;
        if (type == LOAD_CASHING) {
            if (!cacher->isFullyLoaded(req_url)) {
                cacher->deletePage(req_url);
            }
        }
    }
    delete resp_settings;
    delete req_parser;
    delete req_settings;
    delete log;
}

int SocketHandler::getServerSocket() const {
    return server_socket;
}


HandlerType SocketHandler::getType() const {
    return type;
}

bool SocketHandler::isConnectedToServerThisTurn() const {
    return connected_to_server_this_turn;
}

Cacher *SocketHandler::getCacher() const {
    return cacher;
}

size_t SocketHandler::getRespCachePosition() const {
    return resp_cache_position;
}

const std::string &SocketHandler::getReqUrl() const {
    return req_url;
}

void SocketHandler::setHasHeir(bool hasHeir) {
    has_heir = hasHeir;
}












