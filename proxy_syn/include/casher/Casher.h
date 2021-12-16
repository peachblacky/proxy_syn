#pragma once
#ifndef CACHER_H
#define CACHER_H

#include <map>
#include <string>
#include <logger.hpp>
#include <vector>

enum CacheReturn {
    PageNotFound,
    NotEnoughSpace,
    OtherError,
    InvalidPosition,
    OK
};

#define CACHE_CHUNK_SIZE (1024 * 8)

class CachedPage {
private:
    Logger *log;
    bool fully_loaded;
    size_t cur_chunk_position;
//    std::vector<char> page;
    std::vector<std::vector<char>* > page_chunked;

    [[nodiscard]] bool isFullyLoaded() const;

    size_t pageSize();

    CacheReturn appendPage(char *buffer, size_t len);

    CacheReturn acquireDataChunk(char **buffer, size_t &len, size_t position);

    CacheReturn setFullyLoaded();

    CachedPage();

public:
    virtual ~CachedPage();

private:

    friend class Cacher;
};

class Cacher {
private:
    Logger *log;
    std::map<std::string, CachedPage *> pages; //req_url-page_in_cash

public:
    Cacher();

    CacheReturn appendCache(const std::string &url, char *buffer, size_t len);

    CacheReturn setFullyLoaded(const std::string &url);

    CacheReturn acquireChunk(char *buf, size_t &len, const std::string &url, size_t position);

    void deletePage(const std::string &url);

    bool isFullyLoaded(const std::string &url);

    bool isCached(const std::string &url);

    static size_t getChunkSize();

    virtual ~Cacher();
};

#endif // CACHER_H
