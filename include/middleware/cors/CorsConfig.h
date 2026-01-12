#pragma once

#include <vector>
#include <string>

namespace http
{
namespace middleware
{

struct CorsConfig
{
    std::vector<std::string> allowedOrigins;
    std::vector<std::string> allowedMethods;
    std::vector<std::string> allowedHeaders;
    bool allowCredentials = false;
    int maxAge = 3600;

    static CorsConfig defaultConfig()
    {
        CorsConfig config;
        config.allowedOrigins = {"*"};
        config.allowedMethods = {"GET", "POST", "PUT", "DELETE", "OPTIONS"};
        config.allowedHeaders = {"Content-Type", "Authorization"};
        // 设置一个白名单，这些是允许带的，或者说允许放行的
        return config;
    }
};



} // namespace middleware
} // namespace http
