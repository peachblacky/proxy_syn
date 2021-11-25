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
    return found_page->second->is_finished();
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
        return CPNF;
    }
    log.deb(TAGC, "Appending cache");
    return foundPage->second->append_page(buffer, len);
}

CacheReturn CachedPage::append_page(char *buffer, size_t len) {
    try {
        log.deb(TAGP, "Appending page");
        page.append(buffer, len);
        log.deb(TAGP, "Page appended");
    } catch (std::bad_alloc &bad_alloc) {
        return CNESP;
    }
    return COK;
}

CachedPage::CachedPage() : finished(false),
                           log(*new Logger(
                                   Constants::DEBUG,
                                   std::cout)) {}

Cacher::Cacher() :
        log(*new Logger(
                Constants::DEBUG,
                std::cout)) {};

bool CachedPage::is_finished() {
    return finished;
}
