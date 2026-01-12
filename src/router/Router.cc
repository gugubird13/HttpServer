#include "../../include/router/Router.h"

namespace http
{
namespace router
{
    
// 注册路由处理器
void Router::registerHandler(HttpRequest::Method method, const std::string& path, HandlerPtr handler)
{
    RouteKey key{method, path};
    handlers_[key] = std::move(handler);
}

// 注册回调函数形式的处理器
void Router::registerCallback(HttpRequest::Method method, const std::string& path, const HandlerCallback callback)
{
    RouteKey key{method, path};
    callbacks_[key] = callback;
}

bool Router::route(const HttpRequest &req, HttpResponse* resp)
{
    RouteKey key{req.method(), req.path()};

    // 查找处理器
    auto handlerIt = handlers_.find(key);
    if(handlerIt != handlers_.end())
    {
        handlerIt->second->handle(req, resp);
        return true;
    }

    // 查找回调函数
    auto callbackIt = callbacks_.find(key);
    if(callbackIt != callbacks_.end())
    {
        callbackIt->second(req, resp);
        return true;
    }

    // 查找动态路由处理器
    for(const auto &[method, pathRegex, handler] : regexHandlers_)
    {
        std::smatch match;
        std::string pathStr(req.path());
        // 如果方法匹配并且动态路由也匹配，则执行处理器
        if( method == req.method() && std::regex_match(pathStr, match, pathRegex))
        {
            // Extract path parameters and add them to the request
            HttpRequest newReq(req);
            extractPathParameters(match, newReq);
            
            handler->handle(newReq, resp);
            return true;
        }
    }

    // 查找动态路由回调函数
    for(const auto& [method, pathRegex, callback] : regexCallbacks_)
    {
        std::smatch match;
        std::string pathStr(req.path());

        if(method == req.method() && std::regex_match(pathStr, match, pathRegex))
        {
            HttpRequest newReq(req);
            extractPathParameters(match, newReq);
            
            callback(newReq, resp);
            return true;
        }
    }

    return false;
}

}
}