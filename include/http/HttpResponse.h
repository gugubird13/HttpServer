#pragma once

#include <muduo/net/TcpServer.h>

#include <string>


/*
 * 这个类封装了HttpResponse
 * HTTP/1.1 200 OK\r\n
 * Content-Type: text/plain\r\n
 * Content-Length: 16\r\n
 * Connection: keep-alive\r\n
 * \r\n
 * Results for: cpp
 *
 * 因此对应的方法就如下了
 * 同时，我们也需要配合 TcpConnection 去做Buffer的填充，很明显是 outputBuffer_
*/

namespace http
{

class HttpResponse
{
public:
    enum HttpStatusCode
    {
        kUnknown,
        k200Ok = 200,
        k204NoContent = 204,
        k301MovedPermanently = 301,
        k400BadRequest = 400,
        k401Unauthorized = 401,
        k403Forbidden = 403,
        k404NotFound = 404,
        k409Conflict = 409,
        k500InternalServerError = 500,
    };

    HttpResponse(bool close = true)
        : statusCode_(kUnknown)
        , closeConnection_(close)
    {}

    void setVersion(std::string version)
    { httpVersion_ = version; }

    void setStatusCode(HttpStatusCode code)
    { statusCode_ = code; }

    HttpStatusCode getStatusCode() const
    { return statusCode_; }

    void setStatusMessage(const std::string message)
    { statusMessage_ = message; }

    // 用于标记服务器在发送完这个响应后是否要关闭 TCP 连接
    // 通常我们 构造函数，默认是简单连接(HTTP/1.0 客户端)
    // 而对应的  HTTP/1.1 默认是保持连接的, 也就是 keep-alive
    void setCloseConnection(bool on)
    { closeConnection_ = on; }

    bool closeConnection() const
    { return closeConnection_; }

    void setContentType(const std::string& contentType)
    { addHeader("Content-Type", contentType); }

    void setContentLength(uint64_t length)
    { addHeader("Content-Length", std::to_string(length)); }

    void addHeader(const std::string& key, const std::string& value)
    { headers_[key] = value; }

    void setBody(const std::string& body)
    {
        body_ = body;
        // body_ += "\0";
    }

    void setStatusLine(const std::string& version, 
                        HttpStatusCode statusCode,
                        const std::string& statusMessage);

    void setErrorHeader() {}

    void appendToBuffer(muduo::net::Buffer* outputBuf) const;

private:
    std::string             httpVersion_;
    HttpStatusCode          statusCode_;
    std::string             statusMessage_;
    bool                    closeConnection_;
    std::map<std::string, std::string>      headers_;
    std::string             body_;
    bool                    isFile_;
};

}  // namespace http
