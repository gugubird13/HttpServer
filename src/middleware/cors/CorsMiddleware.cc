#include "../../../include/middleware/cors/CorsMiddleware.h"
#include <muduo/base/Logging.h>
#include <algorithm>
// #include <iostream>
#include <sstream>

namespace http
{
namespace middleware
{
    
CorsMiddleware::CorsMiddleware(const CorsConfig& config = CorsConfig::defaultConfig())
    : config_(config)
{}

void CorsMiddleware::before(HttpRequest& request)
{
    LOG_DEBUG << "CorsMiddleware::before - Processing request";

    if(request.method() == HttpRequest::Method::kOptions)
    {
        // 浏览器在发送复杂请求的时候，会发送这个预检请求
        // 只有服务器同意了，才回发送实际的请求
        LOG_INFO << "Processing CORS preflight request";
        HttpResponse response;
        handlePreflightRequest(request, response);
        throw response; // 这里很关键，我这里throw之后，说明我这个请求不需要再传给下一个中间件
        // 不需要再进去Router查找路由了，不需要执行业务逻辑，我直接把结果发回浏览器即可，这是一个非常高效的处理方式
    }
}

void CorsMiddleware::after(HttpResponse& response)
{
    // 实际上，服务器不管是什么类型的请求，这里都要加上这个headers，否则会让浏览器认为：虽然刚刚的options答应了给权限
    // 但是你现在返回给我的数据里写我没有权读取，那么浏览器会拦截这个响应数据，前端代码依然拿不到数据，最后报错
    LOG_DEBUG << "CorsMiddleware::after - Processing response";

    // 直接添加CORS头，简化处理逻辑
    if(!config_.allowedOrigins.empty())
    {
        // 如果允许所有的源
        if(std::find(config_.allowedOrigins.begin(), config_.allowedOrigins.end(), "*") 
                != config_.allowedOrigins.end())
        {
            addCorsHeaders(response, "*");
        }
        else
        {
            // 添加第一个允许的源
            addCorsHeaders(response, config_.allowedOrigins[0]);
        }
    }
}

bool CorsMiddleware::isOriginAllowed(const std::string& origin) const
{
    // 在vector里面查找的逻辑
    // 如果白名单是空的，通常意味着没有设置防， 没有限制
    // 否则先看看有没有通配符
    // 最后再查找origin是不是在白名单里面
    return config_.allowedOrigins.empty() 
            || std::find(config_.allowedOrigins.begin(), config_.allowedOrigins.end(), "*") != config_.allowedOrigins.end()
            || std::find(config_.allowedOrigins.begin(), config_.allowedOrigins.end(), origin) != config_.allowedOrigins.end();
}

void CorsMiddleware::handlePreflightRequest(const HttpRequest& request, HttpResponse& response)
{
    const std::string& origin = request.getHeader("Origin");

    if(!isOriginAllowed(origin))
    {
        LOG_WARN << "Origin not allowed: " << origin;
        response.setStatusCode(HttpResponse::k403Forbidden);
        return;
    }

    // 如果允许，我们需要返回一个response，给这个response加上这个头
    // 并且这个 Option请求是没有body内容的，所以我们返回 204
    addCorsHeaders(response, origin);
    response.setStatusCode(HttpResponse::k204NoContent);
    LOG_INFO << "Preflight request processed successfully";
}

void CorsMiddleware::addCorsHeaders(HttpResponse& response, const std::string& origin)
{
    try
    {
        response.addHeader("Access-Control-Allow-Origin", origin);

        if(config_.allowCredentials)
        {
            response.addHeader("Access-Control-Allow-Credentials", "true");
        }

        if(!config_.allowedMethods.empty())
        {
            response.addHeader("Access-Control-Allow-Methods", join(config_.allowedMethods, ", "));
        }

        if(!config_.allowedHeaders.empty())
        {
            response.addHeader("Access-Control-Allow-Headers", join(config_.allowedHeaders, ", "));
        }        

        response.addHeader("Access-Control-Max-Age", std::to_string(config_.maxAge));

        LOG_DEBUG << "CORS headers added successfully";
    }
    catch(const std::exception& e)
    {
        LOG_ERROR << "Error adding CORS heads: " << e.what();
    }
}

// 工具函数，将字符串数组拼接成单个字符串
std::string CorsMiddleware::join(const std::vector<std::string>& strings, const std::string& delimiter)
{
    std::ostringstream result;
    for(size_t i=0; i<strings.size(); ++i)
    {
        if(i > 0) result << delimiter;
        result << strings[i]; 
    }
    return result.str();
}

} // namespace middleware
} // namespace http


