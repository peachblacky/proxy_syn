#include <Constants.h>
#include <sys/poll.h>
#include "proxy/SocketHandler.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <vector>
#include <netdb.h>
#include <cstring>
#include <unistd.h>
#include <http_parser/http_parser.h>
#include <fcntl.h>

#define TAG "socket_handler"

SocketHandler::SocketHandler(int sockfd, Cacher &casher, std::vector<pollfd> &pfds_ref,
                             std::map<int, Socket *> &sockets_ref) : cacher(casher),
                                                                     client_socket(sockfd),
                                                                     server_socket(-1),
                                                                     log(*new Logger(
                                                                             Constants::DEBUG,
                                                                             std::cout)),
                                                                     type(STANDBY),
                                                                     last_was_value(false),
                                                                     req_in_process(false),
                                                                     req_ready(false),
                                                                     resp_ready(false),
                                                                     req_sent(false),
                                                                     resp_sent(false),
                                                                     poll_fds_ref(pfds_ref),
                                                                     sockets_ref(sockets_ref),
                                                                     req_sent_bytes(0),
                                                                     resp_sent_bytes(0),
                                                                     connected_to_server_this_turn(false) {
    init_response_parser();
    init_request_parser();
}

void SocketHandler::init_request_parser() {
    req_settings = new http_parser_settings;
    http_parser_settings_init(req_settings);
    req_settings->on_url = url_callback;
    req_settings->on_header_field = header_field_callback;
    req_settings->on_header_value = header_value_callback;
    req_settings->on_message_complete = message_complete_callback;
    cur_header_field.clear();
    cur_header_value.clear();
    last_was_value = false;

    req_parser = static_cast<http_parser *>(malloc(sizeof(http_parser)));
    http_parser_init(req_parser, HTTP_REQUEST);
}

void SocketHandler::init_response_parser() {
    resp_settings = new http_parser_settings;
    http_parser_settings_init(resp_settings);
//    resp_settings->on_header_field = header_field_callback;
//    resp_settings->on_header_value = header_value_callback;
    resp_settings->on_message_complete = message_complete_callback;
//    resp_settings->on_body = body_callback;
//    cur_header_field.clear();
//    cur_header_value.clear();
//    last_was_value = false;

    resp_parser = static_cast<http_parser *>(malloc(sizeof(http_parser)));
    http_parser_init(resp_parser, HTTP_RESPONSE);
}

int SocketHandler::url_callback(http_parser *parser, const char *at, size_t length) {
//    std::cout << "Callback triggered : " << "URL" << std::endl;
    auto sh = static_cast<SocketHandler *>(parser->data);
    sh->req_url.append(at, length);
    return 0;
}

int SocketHandler::header_field_callback(http_parser *parser, const char *at, size_t length) {
//    std::cout << "Callback triggered : " << "H FIELD" << std::endl;
    auto sh = static_cast<SocketHandler *>(parser->data);
    if (sh->last_was_value) {
//        std::cout << "Inserting new header : " << "[" << sh->cur_header_field << "]" << "[" << sh->cur_header_value << "]"
//                  << std::endl;
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

int SocketHandler::header_value_callback(http_parser *parser, const char *at, size_t length) {
//    std::cout << "Callback triggered : " << "H VAL" << std::endl;
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

int SocketHandler::message_complete_callback(http_parser *parser) {
    std::cout << "Callback triggered : " << "MESS COMP" << std::endl;
    auto sh = static_cast<SocketHandler *>(parser->data);
    if (sh->parser_mode == REQUEST) {
        std::cout << "Req ready!" << std::endl;
        sh->req_ready = true;
    } else if (sh->parser_mode == RESPONSE) {
        std::cout << "Resp ready!" << std::endl;
        sh->resp_ready = true;
    }

    return 0;
}

int SocketHandler::body_callback(http_parser *parser, const char *at, size_t length) {
//    std::cout << "Callback triggered : " << "H COMP" << std::endl;
    auto sh = static_cast<SocketHandler *>(parser->data);
    if (sh->parser_mode == RESPONSE) {
        sh->resp_body.append(at, length);
    } else if (sh->parser_mode == REQUEST) {
        sh->req_body.append(at, length);
    }
    return 0;
}

bool SocketHandler::receive_and_parse_request() {
    char request_buff[req_buff_capacity]{};

    log.deb(TAG, "Receiving request from " + std::to_string(client_socket));
    ssize_t recved = recv(client_socket, request_buff, req_buff_capacity, 0);
    log.deb(TAG, "Recved " + std::to_string(recved));
    if (recved < 0) {
        log.err(TAG, "Error receiving request from client " + std::to_string(client_socket));
        return false;
    } else if (recved == 0) {
        return false;
    }

    req_in_process = true;

    req_parser->data = this;


//    log.deb(TAG, "Parsing request, reqbuf len is " + std::to_string(recved));
    size_t nparsed = http_parser_execute(req_parser, req_settings, request_buff, recved);
    if (nparsed != recved) {
        log.err(TAG, "Error parsing request from " + std::to_string(client_socket));
        return false;
    }

    request_full.append(request_buff, recved);

    parsed_url = new http_parser_url;
    http_parser_url_init(parsed_url);
    log.deb(TAG, "Parsing req_url : " + req_url);
    if (http_parser_parse_url(req_url.data(), req_url.length(), 0, parsed_url)) {
        log.err(TAG, "Error parsing req_url in request from " + std::to_string(client_socket));
        return false;
    }
    log.deb(TAG, "Parsed req_url : " + req_url);

//    log.deb(TAG, request_buff);

    return true;
}

bool SocketHandler::receive_and_parse_response() {
    //TODO also write received data to cash
    char response_buff[resp_buff_capacity];

    log.deb(TAG, "Receiving resposne from " + std::to_string(server_socket));
    ssize_t recved = recv(server_socket, response_buff, resp_buff_capacity, 0);
//    log.deb(TAG, "Recved " + std::to_string(recved));
    if (recved < 0) {
        log.err(TAG, "Error receiving response from client " + std::to_string(server_socket));
        return false;
    } else if (recved == 0) {
        return false;
    }

//    resp_in_process = true;

    resp_parser->data = this;

//    log.deb(TAG, "Parsing response");
    size_t nparsed = http_parser_execute(resp_parser, resp_settings, response_buff, recved);
    if (nparsed != recved) {
        log.err(TAG, "Error parsing response from " + std::to_string(server_socket) + " , cause only " +
                     std::to_string(nparsed) + " was parsed.\n" + "Errno is " +
                     std::to_string(resp_parser->http_errno));
        return false;
    }
//    log.deb(TAG, "Response parsed");


    response_full.append(response_buff, recved);
    if (type == LOAD_CASHING) {
        log.deb(TAG, "Appending cache");
        auto cache_return = cacher.appendCache(req_url, response_buff, recved);
        log.deb(TAG, "Written new data in cache");
        if (cache_return == CNESP) {
            log.err(TAG, "Not enough space in cache");
            type = LOAD_TRANSIENT;
        }
    }
    return true;
}

bool SocketHandler::acquire_handler_type() {
//    log.deb(TAG, "Acquiring handler type on socket " + std::to_string(client_socket));
    if (req_url.empty()) {
        log.err(TAG, "Error acquiring handler type");
        return false;
    }
    if (cacher.is_cached(req_url)) {
        type = CASH;
    } else {
        type = LOAD_CASHING;
    }
//    log.deb(TAG, "Type acquired");
    return true;
}

bool SocketHandler::connect_to_server() {
    addrinfo *ailist;
    addrinfo hint{};
    bzero(&hint, sizeof(hint));
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_family = PF_UNSPEC;

    std::string url_host;
    url_host.append(req_url, parsed_url->field_data[1].off, parsed_url->field_data[1].len);


    log.deb(TAG, "Trying getting addr of server on req_url " + url_host);
    if (getaddrinfo(url_host.c_str(), "80", &hint, &ailist)) {
        log.err(TAG, "Error getting server address data " + std::to_string(errno));
        return false;
    }
    log.deb(TAG, "Successfully got addrs of server");

    for (auto aip = ailist; aip != nullptr; aip = aip->ai_next) {
        int sock;

        if ((sock = socket(aip->ai_family, aip->ai_socktype, aip->ai_protocol)) < 0) {
            log.err(TAG, "Failed to create socket for server");
            return false;
        }
        if (connect(sock, aip->ai_addr, aip->ai_addrlen) == 0) {
            log.info(TAG,
                     "Successfully connected to req_url " + req_url + " on client " + std::to_string(client_socket));
            server_socket = sock;
            pollfd new_pollfd{};
            new_pollfd.fd = sock;
            new_pollfd.events = POLLIN | POLLOUT;
            poll_fds_ref.push_back(new_pollfd);
            log.deb(TAG, "Pushed new server " + std::to_string(new_pollfd.fd) + " to pollfd vector");
            auto socket_wrap = new Socket(sock, aip->ai_addr, aip->ai_addrlen, SERVER);
            sockets_ref.insert(std::pair<int, Socket *>(sock, socket_wrap));
            connected_to_server_this_turn = true;
            return true;
        }
        close(sock);
    }
    log.err(TAG, "Couldn't connect to server by req_url " + req_url + " on socket " + std::to_string(client_socket));
    return false;
}

bool SocketHandler::send_request() {
    if (!connect_to_server()) {
        return false;
    }
    log.info(TAG,
             "Sending request from " + std::to_string(client_socket) + " to server " + std::to_string(server_socket));
//    log.deb(TAG, request_buff);
    if (send(server_socket, request_full.c_str(), request_full.size(), 0) == -1) {
        log.err(TAG, "Error sending request from " + std::to_string(client_socket) + " to server " +
                     std::to_string(server_socket));
        return false;
    }
    log.info(TAG, "Request sent to " + std::to_string(server_socket));
    req_sent = true;
    return true;
}

bool SocketHandler::send_response() {
    log.info(TAG,
             "Sending response from " + std::to_string(server_socket) + " to client " + std::to_string(client_socket));
    auto left_to_send = (response_full.size() - resp_sent_bytes);
    auto amount_to_send = left_to_send > BUFSIZ ? BUFSIZ * (left_to_send / BUFSIZ) : left_to_send;
    ssize_t sent = send(client_socket, response_full.c_str() + resp_sent_bytes, amount_to_send, 0);
    if (sent == -1) {
        log.err(TAG, "Error sending request from " + std::to_string(client_socket) + " to server " +
                     std::to_string(server_socket));
        return false;
    }
    resp_sent_bytes += sent;
//    log.deb(TAG, "resp sent " + std::to_string(resp_sent_bytes));
    if (resp_sent_bytes == response_full.size()) {
        log.deb(TAG, "Resp all sent " + std::to_string(resp_sent_bytes));
//        log.deb(TAG, "\n" + response_full);
        resp_sent = true;
    }
    return true;
}

//bool SocketHandler::receive_response() {
//    log.info(TAG, "Receiving response from " + std::to_string(server_socket));
//    if (type == LOAD_CASHING) {
//        return send_response();
//    } else if (type == CASH) {
//        //TODO
//    }
//}

bool SocketHandler::work(short revents, SocketType sock_type) {
//    log.info(TAG, "Resp sent is " + std::to_string(resp_sent));
    connected_to_server_this_turn = false;
    if (resp_sent && req_sent) {
        return false;
    }

    if (revents & POLLIN) {
        if (revents & (POLLHUP | POLLERR)) {
            return false;
        }
        if (sock_type == CLIENT && !req_ready) {
            parser_mode = REQUEST;
            if (!receive_and_parse_request()) {
                return false;
            }
        } else if (sock_type == SERVER && !resp_ready) {
            parser_mode = RESPONSE;
            log.deb(TAG, "Receiving response");
            if (!receive_and_parse_response()) {
                return false;
            }
        }
    }

    if (req_ready && !req_sent) {
        if (type == STANDBY) {
            if (!acquire_handler_type()) {
                return false;
            }
        }
        if (type == LOAD_CASHING) {
            if (!send_request()) {
                return false;
            }
        }
    }

//    if (type == LOAD_CASHING && resp_ready && !resp_sent) {//TODO transient mode, not wait until all received
//        if (revents | POLLOUT) {
//            if (!send_response()) {
//                return false;
//            }
//        }
//    } else if (type == CASH) {
//        //TODO
//    }


    return true;
}

SocketHandler::~SocketHandler() {
    log.err(TAG, "Handler for " + std::to_string(client_socket) + "terminates");
    close(client_socket);
//    close(server_socket);
    free(resp_parser);
    free(resp_settings);
    free(req_parser);
    free(req_settings);
}

int SocketHandler::getServerSocket() const {
    return server_socket;
}

int SocketHandler::getClientSocket() const {
    return client_socket;
}

HandlerType SocketHandler::getType() const {
    return type;
}

bool SocketHandler::isConnectedToServerThisTurn() const {
    return connected_to_server_this_turn;
}








