#pragma once 

// 写完HttpRequest 之后，我们知道HttpRequest类是用于封装 HTTP 请求报文信息的，那么
// 我们应该如何验证客户端发送过来的这个 HTTP 报文的格式和基本内容是否符合规范呢？
// 这时就轮到 HttpContext 发挥作用了

// 这里的顺序应该是 HttpContext 先于 HttpRequest 
// HttpContext --> HttpRequest 每一个连接维持一个HttpContext 
// HttpRequest 是被HttpContext 填充的 "数据对象"
// 因此 HttpContext 属于 连接级别的 解析器/状态机，主要用于维护解析的状态

#include <iostream>

#include <muduo/net/TcpServer.h>

#include "HttpRequest.h"

namespace http
{

class HttpContext
{
public:
    enum HttpRequestParseState
    {
        kExpectRequestLine, // 解析请求行
        kExpectHeaders,     // 解析请求头
        kExpectBody,        // 解析请求体
        kGotAll,            // 解析完成
    };

    HttpContext()
        : state_(kExpectRequestLine)  // 增量解析，初始解析状态为解析请求行
    {}

    bool parseRequest(muduo::net::Buffer* buf, muduo::Timestamp receiveTime);
    bool gotAll() const
    { return state_ == kGotAll; }

    void reset()
    {
        state_ = kExpectRequestLine;
        HttpRequest dummyData;
        request_.swap(dummyData);
    }

    const HttpRequest& request() const
    { return request_; }

    HttpRequest& request()
    { return request_; }

private:
    bool processRequestLine(const char* begin, const char* end);
    HttpRequestParseState   state_;
    HttpRequest             request_;
};

} // namespace http
