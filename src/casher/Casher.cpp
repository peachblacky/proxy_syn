//
// Created by Sibdroid on 11/8/2021.
//

#include <Constants.h>
#include "casher/Casher.h"


#define TAGP "CACHE PAGE"
#define TAGC "CACHE"

bool Cacher::isCached(const std::string &url) {
    if (pages.find(url) == pages.end()) {
        return false;
    } else {
        return true;
    }

}

bool Cacher::isFullyLoaded(const std::string &url) {
    auto found_page = pages.find(url);
    if (found_page == pages.end()) {
        return false;
    }
    return found_page->second->isFullyLoaded();
}

CacheReturn Cacher::appendCache(const std::string &url, char *buffer, size_t len) {
    if (pages.find(url) == pages.end()) {
        log->deb(TAGC, "Inserting new page");
        pages.insert(
                std::pair<std::string, CachedPage *>(url, new CachedPage())
        );
    }
    auto foundPage = pages.find(url);
    if (foundPage == pages.end()) {
        return CacheReturn::PageNotFound;
    }
    log->deb(TAGC, "Appending cache");
    return foundPage->second->appendPage(buffer, len);
}

CacheReturn Cacher::setFullyLoaded(const std::string &url) {
    auto find_result = pages.find(url);
    if (find_result == pages.end()) {
        return CacheReturn::PageNotFound;
    }
    log->info(TAGC, "Cache for url " + url + " is sat fully loaded");
    return find_result->second->setFullyLoaded();
}

Cacher::Cacher() :
        log(new Logger(
                Constants::DEBUG,
                std::cerr)) {}

size_t Cacher::getChunkSize() {
    return CACHE_CHUNK_SIZE;
}


CacheReturn Cacher::acquireChunk(char *buf, size_t &len, const std::string &url, size_t position) {
    auto find_result = pages.find(url);
    if (find_result == pages.end()) {
        return CacheReturn::PageNotFound;
    }
    auto found_page = find_result->second;
    if (position > found_page->pageSize()) {
        return CacheReturn::InvalidPosition;
    }
    return found_page->acquireDataChunk(&buf, len, position);
}

void Cacher::deletePage(const std::string &url) {
    delete pages.at(url);
    pages.erase(url);
}

Cacher::~Cacher() {
    delete log;
    for (auto &page: pages) {
        delete page.second;
    }
    pages.clear();
}

CachedPage::CachedPage() : fully_loaded(false),
                           log(new Logger(
                                   Constants::DEBUG,
                                   std::cerr)),
                           cur_chunk_position(0) {}

bool CachedPage::isFullyLoaded() const {
    return fully_loaded;
}

size_t CachedPage::pageSize() {
    size_t size = 0;
    for (auto &i: page_chunked) {
        size += i->size();
    }
    return size;
}

CacheReturn CachedPage::setFullyLoaded() {
    fully_loaded = true;
    return CacheReturn::OK;
}

CacheReturn CachedPage::appendPage(char *buffer, size_t len) {
    try {
        log->deb(TAGP, "Appending page");
        if (page_chunked.begin() == page_chunked.end()) {
            page_chunked.push_back(new std::vector<char>);
            log->deb(TAGP, "New vector size is " + std::to_string(page_chunked.size()));
        }
        size_t bytes_available_to_write = CACHE_CHUNK_SIZE - page_chunked.back()->size();
        log->deb(TAGP, "Available to write " + std::to_string(bytes_available_to_write));\
        log->deb(TAGP, "Going to write " + std::to_string(len));
        size_t amount_to_send = 0;
        size_t appended = 0;
        while (len > 0) {
            amount_to_send = len > bytes_available_to_write ?
                             bytes_available_to_write : len;
            auto last_chunk = page_chunked.back();
            last_chunk->insert(last_chunk->end(), buffer + appended, buffer + appended + amount_to_send);
            len -= amount_to_send;
            appended += amount_to_send;
            if (amount_to_send == bytes_available_to_write) {
                page_chunked.push_back(new std::vector<char>);
                log->deb(TAGP, "New vector size is " + std::to_string(page_chunked.size()));
                bytes_available_to_write = CACHE_CHUNK_SIZE;
            } else {
                break;
            }
        }
        log->deb(TAGP, "Page appended");
    } catch (std::bad_alloc &bad_alloc) {
        return CacheReturn::NotEnoughSpace;
    }
    return CacheReturn::OK;
}


CacheReturn CachedPage::acquireDataChunk(char **buffer, size_t &len, size_t position) {
    auto page_size = pageSize();
    if (position > page_size) {
        return CacheReturn::InvalidPosition;
    }
    log->deb(TAGP, std::to_string(position) + " and " + std::to_string(page_size));
    log->deb(TAGP, "Vector size is " + std::to_string(page_chunked.size()));
    log->deb(TAGP, "Truing to access position " + std::to_string(position / CACHE_CHUNK_SIZE));
    auto position_chunk = page_chunked[position / CACHE_CHUNK_SIZE];
    auto chunk_position = position % CACHE_CHUNK_SIZE;
    len = position_chunk->size() - chunk_position;
    log->deb(TAGP, "LEN IS " + std::to_string(len));
    for (size_t i = 0; i < len; i++) {
        (*buffer)[i] = position_chunk->at(i + chunk_position);
    }
    return CacheReturn::OK;
}

CachedPage::~CachedPage() {
    delete log;
}
