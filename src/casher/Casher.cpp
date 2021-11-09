//
// Created by Sibdroid on 11/8/2021.
//

#include "casher/Casher.h"

Casher::Casher() = default;

bool Casher::is_cashed(std::string url) {
    if(pages.find(url) == pages.end()) {
        return false;
    } else {
        return true;
    }
}
