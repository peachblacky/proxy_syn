#include <Constants.h>
#include <sys/poll.h>
#include "proxy/SocketHandler.h"
#include <sys/socket.h>
#include <sys/types.h>

#define TAG "cash_handler"

SocketHandler::SocketHandler(int sockfd, Casher &casher) : casher(casher), client_socket(sockfd),
                                                           log(*new Logger(Constants::DEBUG, std::cout)),
                                                           type(STANDBY), last_was_value(false), req_in_process(false),
                                                           req_ready(false) {
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
    std::cout << "Callback triggered : " << "URL" << std::endl;
    auto sh = static_cast<SocketHandler *>(parser->data);
    sh->url.append(at, length);
    return 0;
}

int SocketHandler::header_field_callback(http_parser *parser, const char *at, size_t length) {
    std::cout << "Callback triggered : " << "H FIELD" << std::endl;
    auto sh = static_cast<SocketHandler *>(parser->data);
    if (sh->last_was_value) {
        std::cout << "Inserting new header : " << "[" << sh->cur_header_field << "]" << "[" << sh->cur_header_value << "]"
                  << std::endl;
        sh->headers.insert(std::pair<std::string, std::string>(sh->cur_header_field, sh->cur_header_value));

        sh->cur_header_field.clear();
        sh->cur_header_value.clear();

        sh->cur_header_field.append(at, length);
    } else {
        if(!sh->cur_header_value.empty()) {
            return 1;
        }

        sh->cur_header_field.append(at, length);
    }
    sh->last_was_value = false;
    return 0;
}

int SocketHandler::header_value_callback(http_parser *parser, const char *at, size_t length) {
    std::cout << "Callback triggered : " << "H VAL" << std::endl;
    auto sh = static_cast<SocketHandler *>(parser->data);
    if (!sh->last_was_value) {
        if(!sh->cur_header_value.empty()) {
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
    std::cout << "Callback triggered : " << "H COMP" << std::endl;
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

    size_t nparsed = http_parser_execute(parser, settings, req_buf + req_buf_len, recved);
    if (nparsed != recved) {
        log.err(TAG, "Error parsing request from " + std::to_string(client_socket));
        return false;
    }
    //TODO maybe need to pass 0 to parser
    return true;
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

    if (revents & POLLOUT) {
        //TODO implement
    }

    return true;
}
