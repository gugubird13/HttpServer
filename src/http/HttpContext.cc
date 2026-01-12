#include "../../include/http/HttpContext.h"

using namespace muduo;
using namespace muduo::net;

namespace http
{

bool HttpContext::parseRequest(muduo::net::Buffer* buf, muduo::Timestamp receiveTime)
{
    bool ok = true;  // 解析每行请求格式是否正确
    bool hasMore = true;
    while(hasMore)
    {
        if(state_ == kExpectRequestLine)
        {
            const char* crlf = buf->findCRLF();   // 注意这个返回值边界可能有错
            if(crlf)
            {
                ok = processRequestLine(buf->peek(), crlf);
                if (ok) // 如果解析 请求行成功
                {
                    request_.setReceiveTime(receiveTime);
                    buf->retrieveUntil(crlf + 2);   // 读取完之后，就 retrieve , 参数指定的是 end ，retrieve end - peek()
                    state_ = kExpectHeaders;
                }
                else{ // 解析没有成功
                    hasMore = false;
                }
            } 
            else{ // 没有找到这个边界
                hasMore = false;
            }
        }
        else if(state_ == kExpectHeaders)
        {
            const char* crlf = buf->findCRLF();
            if(crlf) // 如果找到这个边界
            {
                const char* colon = std::find(buf->peek(), crlf, ':');    // 找 冒号 :
                if(colon < crlf)
                {
                    request_.addHeader(buf->peek(), colon, crlf);
                }
                else if(buf->peek() == crlf) 
                {
                    // 说明当前行就只有 CRLF 了， 因为我们找到了 CRLF并且当前第一个也是CRLF。
                    // 说明接下来如果有 body的话，需要设置body了，但是有些方法没有body，这里判断并设置标记state_
                    // 根据当前的请求方法和 Content-Length 来判断是否需要继续读取 body
                    if(request_.method() == HttpRequest::kPost ||
                        request_.method() == HttpRequest::kPut)
                    {
                        std::string contentLength = request_.getHeader("Content-Length");
                        if(!contentLength.empty())
                        {
                            request_.setContentLength(std::stoi(contentLength));
                            if(request_.contentLength() > 0)
                            {
                                state_ = kExpectBody;
                            }
                            else{
                                state_ = kGotAll;
                                hasMore = false;
                            }
                        }
                        else{
                            // POST/PUT 请求没有 Content-Length. 是 HTTP 语法错误
                            ok = false;
                            hasMore = false;
                        }
                    }
                    else{
                        // GET/HEAD/DELETE 等方法直接完成(没有请求体)
                        state_ = kGotAll;
                        hasMore = false;
                    }
                }
                else{
                    ok = false; // Header 行格式错误
                    hasMore = false;
                }
                buf->retrieveUntil(crlf + 2);
            } 
            else
            {
                hasMore = false;
            }
        }
        else if(state_ == kExpectBody)
        {
            // 检查缓冲区是否有足够的数据
            if(buf->readableBytes() < request_.contentLength())
            {
                hasMore = false; // 数据不完整，等待更多数据
                return true;
            }

            // 否则我们可以读，但是只能读取指定的长度: Content-Length 指定的长度
            std::string body(buf->peek(), buf->peek() + request_.contentLength());
            request_.setBody(body);

            // 准确移动指针
            buf->retrieve(request_.contentLength());

            state_ = kGotAll;
            hasMore = false;
        }
    }
    return ok;   // ok 为 false 代表报文语法解析错误
}

// 解析请求行
// ----------------------------------------------------------
// GET /search?keyword=cpp&page=2 HTTP/1.1
// Host: 127.0.0.1:8000
// User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) ...
// Accept: text/html,application/xhtml+xml...
// Connection: keep-alive
//
// ----------------------------------------------------------
bool HttpContext::processRequestLine(const char* begin, const char* end)
{
    bool succeed = false;
    const char* start = begin;
    const char* space = std::find(start, end, ' ');
    if(space != end && request_.setMethod(start, space))
    {
        // method 设置好了之后，就要设置后续的参数了，比如 请求路径 请求参数等等信息
        start = space + 1;
        space = std::find(start, end, ' ');
        if(space != end)
        {
            const char* argumentStart = std::find(start, space, '?');
            if(argumentStart != space) // 请求带参数
            {
                request_.setPath(start, argumentStart); // 注意这些返回值边界
                request_.setQueryParameters(argumentStart+1, space);
            }
            else{
                // 请求不带参数
                request_.setPath(start, space);
            }

            start = space + 1;
            succeed = ((end - start == 8) && std::equal(start, end - 1, "HTTP/1."));
            if(succeed)
            {
                if(*(end - 1) == '1')
                {
                    request_.setVersion("HTTP/1.1");
                }
                else if(*(end - 1) == '0')
                {
                    request_.setVersion("HTTP/1.0");
                }
                else
                {
                    succeed = false;
                }
            }
        }

    }
    return succeed;
}


} // namespace http
