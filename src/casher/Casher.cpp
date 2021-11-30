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
    log->deb(TAGC, "Page for " + url + " is " + (found_page->second->isFullyLoaded() ? "true" : "false"));
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
    return found_page->acquireDataChunk(buf, len, position);
}

void Cacher::deletePage(const std::string &url) {
    delete pages.at(url);
    pages.erase(url);
}

Cacher::~Cacher() {
    delete log;
    for(auto &page: pages) {
        delete page.second;
    }
    pages.clear();
}

CachedPage::CachedPage() : fully_loaded(false),
                           log(new Logger(
                                   Constants::DEBUG,
                                   std::cerr)) {}

bool CachedPage::isFullyLoaded() const {
    return fully_loaded;
}

size_t CachedPage::pageSize() {
    return page.size();
}

CacheReturn CachedPage::setFullyLoaded() {
    fully_loaded = true;
    return CacheReturn::OK;
}

CacheReturn CachedPage::appendPage(char *buffer, size_t len) {
    try {
        log->deb(TAGP, "Appending page");
        std::vector<char> insert_buf(buffer, buffer + len);
        for (size_t i = 0; i < len; i++) {
            page.push_back(buffer[i]);
        }
        log->deb(TAGP, "Page appended");
    } catch (std::bad_alloc &bad_alloc) {
        return CacheReturn::NotEnoughSpace;
    }
    return CacheReturn::OK;
}



CacheReturn CachedPage::acquireDataChunk(char *buffer, size_t &len, size_t position) {
    if (position > page.size()) {
        return CacheReturn::InvalidPosition;
    }
    len = (page.size() - position) > CACHE_CHUNK_SIZE ? CACHE_CHUNK_SIZE : (page.size() - position);
    for (size_t i = 0; i < len; i++) {
        buffer[i] = page[position + i];
    }
    return CacheReturn::OK;
}

CachedPage::~CachedPage() {
    delete log;
}
