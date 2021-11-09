#pragma once
#ifndef CASHER_H
#define CASHER_H

#include <map>
#include <string>

    class CashedPage
    {
    private:
        bool finished;

    public:
        bool is_fully_loaded(std::string url);
        bool is_finished();

        CashedPage();
        ~CashedPage();
    };

    class Casher
    {
    private:
        std::map<std::string, CashedPage*> pages;

    public:
        bool is_cashed(std::string url);
        Casher();
    };
#endif // CASHER_H