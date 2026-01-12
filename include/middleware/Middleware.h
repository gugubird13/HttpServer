#pragma once 

#include "../../include/http/HttpRequest.h"
#include "../../include/http/HttpResponse.h"

namespace http
{
namespace middleware
{

class Middleware
{
public:
    virtual ~Middleware() = default;

    // 一半来说，一个middle ware 包括三个基本函数：请求前处理， 相应后处理以及 next
    // 请求前处理
    virtual void before(HttpRequest& request) = 0;

    // 响应后处理
    virtual void after(HttpResponse& resp) = 0;

    // 设置下一个中间件
    void setNext(std::shared_ptr<Middleware> next)
    {
        nextMiddleware_ = next;
    }

protected: // 设置 protect 方便子类可以直接把这个成员变量拿来用
    std::shared_ptr<Middleware> nextMiddleware_;
};

} // namespace middleware
} // namespace http
