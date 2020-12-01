#pragma once

#include "odbc/config.hpp"
#include <string>

namespace odbc {

class KeywordDB;

class TGCPlugin
{
public:
    TGCPlugin() = delete;
    TGCPlugin(const TGCPlugin& other) = delete;
    TGCPlugin(TGCPlugin&& other) = default;
    ~TGCPlugin();

    static TGCPlugin* load(const char* filename);

    bool loadKeywords(KeywordDB& db) const;

private:
    TGCPlugin(void* handle, const std::string& pluginName) : handle_(handle), pluginName_(pluginName) {}

    void* handle_;
    std::string pluginName_;
};

}
