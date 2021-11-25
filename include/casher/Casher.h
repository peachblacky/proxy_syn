#pragma once
#ifndef CACHER_H
#define CACHER_H

#include <map>
#include <string>
#include <logger.hpp>

enum CacheReturn {
    COK, // OK
    CNESP, // not enough space
    CPNF, // page not found
    COTH // other error
};

class CachedPage {
private:
    Logger log;
    bool finished;
    std::string page;
public:
    bool is_finished();

    CacheReturn append_page(char *buffer, size_t len);

    CachedPage();
};

class Cacher {
private:
    Logger log;
    std::map<std::string, CachedPage *> pages; //req_url-page_in_cash
//        std::map<int, std::string> subscribers;

public:
    CacheReturn appendCache(const std::string& url, char *buffer, size_t len);

    bool is_fully_loaded(const std::string &url);

    bool is_cached(const std::string &url);

    Cacher();
};

#endif // CACHER_H