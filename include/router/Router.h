#pragma once

#include "../../include/http/HttpRequest.h"
#include "RouterHandler.h"

#include <functional>
#include <memory>
#include <vector>
#include <regex>

namespace http
{
namespace router
{

// 回调函数有两种形式：
// 选择注册对象式的路由处理器还是注册回调函数式的处理器取决于处理器执行的复杂程度
// 注册对象式的路由处理器（对象可以封装多个相关函数）
// 上述逻辑都是一样的，只不过是存在方式不一样，如果是对象式的，就是把回调函数封装成了某种对象，比如 struct 或者 ptr
class Router
{
public:
    // 两种回调定义格式
    using HandlerPtr = std::shared_ptr<RouterHandler>;
    using HandlerCallback = std::function<void(const HttpRequest &, HttpResponse*)>;

    struct RouteKey
    {
        HttpRequest::Method method;
        std::string path;

        bool operator == (const RouteKey& other) const{
            return method == other.method && path == other.path;
        }
    };

    // 为 RouteKey 定义哈希函数
    struct RouteKeyHash
    {
        size_t operator()(const RouteKey& key) const{
            size_t methodHash = std::hash<int>{} (static_cast<int> (key.method));
            size_t pathHash   = std::hash<std::string>{} (key.path);
            return methodHash * 31 + pathHash;
        }
    };  

    // 注册路由处理器
    void registerHandler(HttpRequest::Method method, const std::string& path, HandlerPtr handler);

    // 注册回调函数形式的处理器
    void registerCallback(HttpRequest::Method method, const std::string& path, const HandlerCallback callback);

    // 注册动态路由处理器
    void addRegexHandler(HttpRequest::Method method, const std::string& path, HandlerPtr handler)
    {
        std::regex pathRegex = convertToRegex(path);
        regexHandlers_.emplace_back(method, pathRegex, handler);
    }

    // 注册动态路由处理函数
    void addRegexCallback(HttpRequest::Method method, const std::string &path, const HandlerCallback& callback )
    {
        std::regex pathRegex = convertToRegex(path);
        regexCallbacks_.emplace_back(method, pathRegex, callback);
    }

    bool route(const HttpRequest &req, HttpResponse* resp);

private:
    std::regex convertToRegex(const std::string& pathPattern)
    {
        // 将路径模式转换为正则表达式，支持匹配任意路径参数
        std::string regexPattern = "^" + std::regex_replace(pathPattern, std::regex(R"(/:([^/]+))"), R"(/([^/]+))") + "$";
        return std::regex(regexPattern);
    }
    
    // 提取路径参数
    void extractPathParameters(const std::smatch &match, HttpRequest & request)
    {
        // 我们这里假设 第一个match的就是整个的路径，参数从index=1 开始
        for(size_t i = 1; i<match.size(); ++i)
        {
            request.setPathParameters("param" + std::to_string(i), match[i].str());
        }
    }

private:
    // 这俩是为了正则匹配服务的，所以要封装相应的正则
    struct RouteCallbackObj
    {
        HttpRequest::Method method_;
        std::regex pathRegex_;
        HandlerCallback callback_;
        RouteCallbackObj(HttpRequest::Method method, std::regex pathRegex, const HandlerCallback & callback)
            : method_(method), pathRegex_(pathRegex), callback_(callback)
        {}
    };

    struct RouteHandlerObj
    {
        HttpRequest::Method method_;
        std::regex pathRegex_;
        HandlerPtr handler_;
        RouteHandlerObj(HttpRequest::Method method, std::regex pathRegex, HandlerPtr handler)
            : method_(method), pathRegex_(pathRegex), handler_(handler)
        {}
    };

    // 我们这里定义了哈希表，并且定义 key 为 RouteKey， 我们这里一定要对 RouteKey 做 == 重载
    // 同时，我们要定义哈希规则, 也就是hash函数
    std::unordered_map<RouteKey, HandlerPtr, RouteKeyHash>              handlers_;           // 精确匹配
    std::unordered_map<RouteKey, HandlerCallback, RouteKeyHash>         callbacks_;     // 精确匹配

    std::vector<RouteHandlerObj>                                        regexHandlers_;     // 正则匹配
    std::vector<RouteCallbackObj>                                       regexCallbacks_;    // 正则匹配    
};

}   // namespace router
}   // namespace http