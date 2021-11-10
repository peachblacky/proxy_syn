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

SocketHandler::SocketHandler(int sockfd, Casher &casher, std::vector<pollfd> &pfds_ref) : casher(casher),
                                                                                          client_socket(sockfd),
                                                                                          log(*new Logger(
                                                                                                  Constants::DEBUG,
                                                                                                  std::cout)),
                                                                                          type(STANDBY),
                                                                                          last_was_value(false),
                                                                                          req_in_process(false),
                                                                                          req_ready(false),
                                                                                          req_sent(false),
                                                                                          poll_fds_ref(pfds_ref) {
    init_parser();
    req_buf = static_cast<char *>(malloc(req_buf_capacity));
}

void SocketHandler::init_parser() {
    settings = new http_parser_settings;
    settings->on_url = url_callback;
    settings->on_header_field = header_field_callback;
    settings->on_header_value = header_value_callback;
    settings->on_headers_complete = headers_complete_callback;

    parser = static_cast<http_parser *>(malloc(sizeof(http_parser)));
    http_parser_init(parser, HTTP_REQUEST);
}

int SocketHandler::url_callback(http_parser *parser, const char *at, size_t length) {
//    std::cout << "Callback triggered : " << "URL" << std::endl;
    auto sh = static_cast<SocketHandler *>(parser->data);
    sh->url.append(at, length);
    return 0;
}

int SocketHandler::header_field_callback(http_parser *parser, const char *at, size_t length) {
//    std::cout << "Callback triggered : " << "H FIELD" << std::endl;
    auto sh = static_cast<SocketHandler *>(parser->data);
    if (sh->last_was_value) {
//        std::cout << "Inserting new header : " << "[" << sh->cur_header_field << "]" << "[" << sh->cur_header_value << "]"
//                  << std::endl;
        sh->headers.insert(std::pair<std::string, std::string>(sh->cur_header_field, sh->cur_header_value));

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

int SocketHandler::headers_complete_callback(http_parser *parser) {
//    std::cout << "Callback triggered : " << "H COMP" << std::endl;
    auto sh = static_cast<SocketHandler *>(parser->data);
    sh->req_ready = true;
    return 0;
}

bool SocketHandler::receive_request_data() {
    log.deb(TAG, "Receiving request from " + std::to_string(client_socket));
    ssize_t recved = recv(client_socket, req_buf + req_buf_len, req_buf_capacity - req_buf_len, 0);
    log.deb(TAG, "Recved " + std::to_string(recved));
    if (recved < 0) {
        log.err(TAG, "Error receiving request from client " + std::to_string(client_socket));
        return false;
    } else if (recved == 0) {
        return false;
    }

    req_in_process = true;

    parser->data = this;

//    log.deb(TAG, "Parsing request, reqbuf len is " + std::to_string(recved));
    size_t nparsed = http_parser_execute(parser, settings, req_buf + req_buf_len, recved);
    if (nparsed != recved) {
        log.err(TAG, "Error parsing request from " + std::to_string(client_socket));
        return false;
    }


    log.deb(TAG, "Parsing url");
    parsed_url = new http_parser_url;
    http_parser_url_init(parsed_url);
    if (http_parser_parse_url(url.data(), url.length(), 0, parsed_url)) {
        log.err(TAG, "Error parsing url in request from " + std::to_string(client_socket));
        return false;
    }
//    log.deb(TAG, req_buf);

    return true;
}

bool SocketHandler::acquire_handler_type() {
//    log.deb(TAG, "Acquiring handler type on socket " + std::to_string(client_socket));
    if (url.empty()) {
        log.err(TAG, "Error acquiring handler type");
        return false;
    }
    if (casher.is_cashed(url)) {
        type = CASH;
    } else {
        type = LOAD;
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
    url_host.append(url, parsed_url->field_data[1].off, parsed_url->field_data[1].len);


    log.deb(TAG, "Trying getting addr of server on url " + url_host);
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

//        int flags_bu = fcntl(sock, F_GETFL);
//        if (flags_bu == -1) {
//            log.err(TAG, "Failed to get server socket flags");
//        } else {
//            log.deb(TAG, "Got server socket flags");
//        }

//        if (fcntl(sock, F_SETFL, flags_bu | O_NONBLOCK) == -1) {
//            log.err(TAG, "Failed to set listening socket non-block");
//        } else {
//            log.deb(TAG, "Server socket set non-block");
//        }
//        sockaddr_in socket_addr{};
//        memcpy(&socket_addr, (void*)aip->ai_addr, sizeof(socket_addr));
//        socket_addr.sin_port = htons(1111);
//        if(parsed_url->port != 80) {
//            continue;
//        }
        if (connect(sock, aip->ai_addr, aip->ai_addrlen) == 0) {
            log.info(TAG, "Successfully connected to url " + url + " on client " + std::to_string(client_socket));
//            if (fcntl(sock, F_SETFL, flags_bu) == -1) {
//                log.err(TAG, "Failed to backup server socket");
//            } else {
//                log.deb(TAG, "Server socket backed-up successfully");
//            }
            server_socket = sock;
            return true;
        }
        close(sock);
    }
    log.err(TAG, "Couldn't connect to server by url " + url + " on socket " + std::to_string(client_socket));
    return false;
}

bool SocketHandler::send_request() {
    if (!connect_to_server()) {
        return false;
    }
    return true;
//    send();
}

bool SocketHandler::receive_response() {
    if (type == LOAD) {
        //TODO
    } else if (type == CASH) {
        //TODO
    }
}

bool SocketHandler::work(short revents) {
    if (revents & (POLLHUP | POLLERR)) {
        return false;
    }

    if (revents & POLLIN) {
        if (!receive_request_data()) {
            return false;
        }
    }

    if (req_ready) {
        if (type == STANDBY) {
            if (!acquire_handler_type()) {
                return false;
            }
            if (type == LOAD) {
                if (!send_request()) {
                    return false;
                }
            } else {
//                receive_response();
            }
        }
    }
//    if (revents & POLLOUT) {
//        //TODO implement
//    }

    return true;
}

SocketHandler::~SocketHandler() {
    close(client_socket);
//    close(server_socket);
    free(req_buf);
    free(parser);
    free(settings);
}




