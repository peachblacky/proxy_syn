#pragma once

#include <iostream>

class Logger {
private:
    bool is_debug;
    std::ostream& output;

public:
    Logger(bool isDebug, std::ostream &output);

    const void err(const std::string &tag, const std::string &message);
    const void info(const std::string &tag, const std::string &message);
    const void deb(const std::string &tag, const std::string &message);

};