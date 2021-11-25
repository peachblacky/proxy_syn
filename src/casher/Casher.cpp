//
// Created by Sibdroid on 11/8/2021.
//

#include <Constants.h>
#include "casher/Casher.h"


#define TAGP "CACHE PAGE"
#define TAGC "CACHE"

bool Cacher::is_cached(const std::string &url) {
    if (pages.find(url) == pages.end()) {
        return false;
    } else {
        return true;
    }

}

bool Cacher::is_fully_loaded(const std::string &url) {
    auto found_page = pages.find(url);
    if (found_page == pages.end()) {
        return false;
    }
    return found_page->second->is_fully_loaded();
}

CacheReturn Cacher::appendCache(const std::string &url, char *buffer, size_t len) {
    if (pages.find(url) == pages.end()) {
        log.deb(TAGC, "Inserting new page");
        pages.insert(
                std::pair<std::string, CachedPage *>(url, new CachedPage())
        );
    }
    auto foundPage = pages.find(url);
    if (foundPage == pages.end()) {
        return CacheReturn::PageNotFound;
    }
    log.deb(TAGC, "Appending cache");
    return foundPage->second->append_page(buffer, len);
}

CacheReturn Cacher::set_fully_loaded(const std::string &url) {
    auto find_result = pages.find(url);
    if (find_result == pages.end()) {
        CacheReturn::PageNotFound;
    }
    return find_result->second->set_fully_loaded();
}

Cacher::Cacher() :
        log(*new Logger(
                Constants::DEBUG,
                std::cout)) {}

size_t Cacher::get_chunk_size() {
    return CACHE_CHUNK_SIZE;
}

CacheReturn CachedPage::append_page(char *buffer, size_t len) {
    try {
        log.deb(TAGP, "Appending page");
        page.append(buffer, len);
        log.deb(TAGP, "Page appended");
    } catch (std::bad_alloc &bad_alloc) {
        return CacheReturn::NotEnoughSpace;
    }
    return CacheReturn::OK;
}

CachedPage::CachedPage() : fully_loaded(false),
                           log(*new Logger(
                                   Constants::DEBUG,
                                   std::cout)) {}


CacheReturn CachedPage::acquire_data_chunk(char *buffer, ssize_t &len, size_t position) {
    if (position > page.size()) {
        return CacheReturn::InvalidPosition;
    }
    len = (page.size() - position) > CACHE_CHUNK_SIZE ? CACHE_CHUNK_SIZE : (page.size() - position);
    size_t ret = 0;
    try {
        ret = page.copy(buffer, len, position);
    } catch (std::out_of_range &e) {
        return CacheReturn::InvalidPosition;
    }
    if (ret != len) {
        return CacheReturn::OtherError;
    }
    return CacheReturn::OK;
}

CacheReturn Cacher::acquire_chunk(char *buf, ssize_t &len, const std::string &url, size_t position) {
    auto find_result = pages.find(url);
    if (find_result == pages.end()) {
        CacheReturn::PageNotFound;
    }
    auto found_page = find_result->second;
    if (position > found_page->page_size()) {
        return CacheReturn::InvalidPosition;
    }
    return found_page->acquire_data_chunk(buf, len, position);
}


bool CachedPage::is_fully_loaded() const {
    return fully_loaded;
}

size_t CachedPage::page_size() {
    return page.size();
}

CacheReturn CachedPage::set_fully_loaded() {
    fully_loaded = true;
    return CacheReturn::OK;
}

