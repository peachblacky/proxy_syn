//
// Created by Sibdroid on 11/7/2021.
//

#include "logger.hpp"

const void Logger::err(const std::string &tag, const std::string &message) {
    output << "\033[31m" << "[ERROR] " << tag << ": " << message << "\033[0m" << std::endl;
}

const void Logger::deb(const std::string &tag, const std::string &message) {
    if (is_debug) {
        output << "\033[37m" << "[DEBUG] " << tag << ": " << message << "\033[0m" << std::endl;
    }
}

const void Logger::info(const std::string &tag, const std::string &message) {
    output << "\033[32m" << "[INFO] " << tag << ": " << message << "\033[0m" << std::endl;
}

Logger::Logger(bool isDebug, std::ostream &output) : is_debug(isDebug), output(output) {}
