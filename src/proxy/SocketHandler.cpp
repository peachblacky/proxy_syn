#include <Constants.h>
#include <sys/poll.h>
#include "proxy/SocketHandler.h"
#include <sys/socket.h>
#include <sys/types.h>

#define TAG "cash_handler"

SocketHandler::SocketHandler(int sockfd, Casher &casher) : casher(casher), client_socket(sockfd),
                                                           log(*new Logger(Constants::DEBUG, std::cout)),
                                                           type(STANDBY), last_was_value(false), req_coming(false),
                                                           req_ready(0) {
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

    return 0;
}

int SocketHandler::header_field_callback(http_parser *parser, const char *at, size_t length) {
    std::cout << "Callback triggered : " << "H FIELD" << std::endl;
    if(last_was_value) {
        headers.insert(std::pair<std::string, std::string>(cur_header_field, cur_header_value));
        headers
    } else {

    }

    return 0;
}

int SocketHandler::header_value_callback(http_parser *parser, const char *at, size_t length) {
    std::cout << "Callback triggered : " << "H VAL" << std::endl;
    return 0;
}

int SocketHandler::headers_complete_callback(http_parser *parser) {
    std::cout << "Callback triggered : " << "H COMP" << std::endl;
    req_ready = true;
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

    int nparsed = http_parser_execute(parser, settings, req_buf + req_buf_len, recved);
    if (nparsed != recved) {
        log.err(TAG, "Error parsing request");
        return false;
    }
    //TODO maybe need to pass 0 to parser
    //TODO implement parsing
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
