#include "../../include/http/HttpResponse.h"

namespace http
{

/*
 * 这个类封装了HttpResponse
 * HTTP/1.1 200 OK\r\n
 * Content-Type: text/plain\r\n
 * Content-Length: 16\r\n
 * Connection: keep-alive\r\n
 * \r\n
 * Results for: cpp
*/
void HttpResponse::appendToBuffer(muduo::net::Buffer* outputBuf) const
{
    // 这个用于将HttpResponse 封装的信息序列化输出
    char buf[32];
    // 这里不把状态信息比如 ok 这种的放到格式化字符串里面，因为这个信息有长有短，不方便定义一个固定大小的内存存储
    // snprintf(buf, sizeof buf, "%s %d", httpVersion_.c_str(), statusCode_);
    snprintf(buf, sizeof buf, "%s %d", "HTTP/1.1", statusCode_);

    outputBuf->append(buf);
    outputBuf->append(statusMessage_); // append 状态信息
    outputBuf->append("\r\n");

    if(closeConnection_)   // 为什么不放到headers_ 里面？ 可能用户的这个headers_ 并没有这个信息
    {
        outputBuf->append("Connection: close\r\n");
    }
    else{
        outputBuf->append("Connection: Keep-Alive\r\n");
    }

    for(const auto& header : headers_)
    {
        outputBuf->append(header.first);
        outputBuf->append(": ");
        outputBuf->append(header.second);
        outputBuf->append("\r\n");
    }

    outputBuf->append("\r\n");
    outputBuf->append(body_);
}


void HttpResponse::setStatusLine(const std::string& version, 
                        HttpStatusCode statusCode,
                        const std::string& statusMessage)
{
    httpVersion_ = version;
    statusCode_ = statusCode;
    statusMessage_ = statusMessage;
}

} // namespace http
