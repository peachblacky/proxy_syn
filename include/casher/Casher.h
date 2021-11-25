#pragma once
#ifndef CACHER_H
#define CACHER_H

#include <map>
#include <string>
#include <logger.hpp>

enum class CacheReturn {
    PageNotFound,
    NotEnoughSpace,
    OtherError,
    InvalidPosition,
    OK // OK
};

#define CACHE_CHUNK_SIZE 8192

class CachedPage {
private:
    Logger log;
    bool fully_loaded;
    std::string page;

    bool is_fully_loaded() const;

    size_t page_size();

    CacheReturn append_page(char *buffer, size_t len);

    CacheReturn acquire_data_chunk(char *buffer, ssize_t& len, size_t position);

    CacheReturn set_fully_loaded();

    CachedPage();

    friend class Cacher;
};

class Cacher {
private:
    Logger log;
    std::map<std::string, CachedPage *> pages; //req_url-page_in_cash
//        std::map<int, std::string> subscribers;

public:
    CacheReturn appendCache(const std::string& url, char *buffer, size_t len);

    CacheReturn set_fully_loaded(const std::string& url);

    CacheReturn acquire_chunk(char* buf, ssize_t& len, const std::string& url, size_t position);

    bool is_fully_loaded(const std::string &url);

    bool is_cached(const std::string &url);

    Cacher();

    static size_t get_chunk_size();
};

#endif // CACHER_H