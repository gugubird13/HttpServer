#include "../../include/http/HttpRequest.h"

namespace http
{

void HttpRequest::setReceiveTime(muduo::Timestamp t)
{
    receiveTime_ = t;
}

bool HttpRequest::setMethod(const char* start, const char* end)
{
    assert(method_ == kInvalid);

    // range 构造方法，通过迭代器构造
    std::string m(start, end);  // [start, end)
    if( m == "GET")
    {
        method_ = kGet;
    }
    else if(m == "POST")
    {
        method_ = kPost;
    }
    else if(m == "PUT")
    {
        method_ = kPut;
    }
    else if(m == "DELETE")
    {
        method_ = kDelete;
    }
    else if(m == "OPTIONS")
    {
        method_ = kOptions;
    }
    else
    {
        method_ = kInvalid;
    }

    return method_ != kInvalid;
}

void HttpRequest::setPath(const char* start, const char* end)
{   
    path_.assign(start, end); // 也是range 拷贝方法
}

void HttpRequest::setPathParameters(const std::string &key, const std::string &value)
{
    pathParameters_[key] = value;
}

std::string HttpRequest::getPathParameters(const std::string& key) const
{
    auto it = pathParameters_.find(key);
    if( it != pathParameters_.end())
    {
        return it->second;
    }
    // 没找到我们就返回空
    return "";
}

// 这个是从问号后面去分割参数
// 比如我在一个浏览器里面输入: http://127.0.0.1:8000/search?keyword=cpp 
// 那么浏览器就会在底层构建如下的文本给你的服务器:

// ----------------------------------------------------------
// GET /search?keyword=cpp&page=2 HTTP/1.1
// Host: 127.0.0.1:8000
// User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) ...
// Accept: text/html,application/xhtml+xml...
// Connection: keep-alive
//
// ----------------------------------------------------------
// (GET 请求通常没有Body, 最后一个是空行结束)

// 那么通常当我们的服务器收到这样的数据之后, HTTP 解析器 Parser 会拆解这段文本, 并调用 HttpRequest 类的方法来填充数据
void HttpRequest::setQueryParameters(const char* start, const char* end)
{
    // 所谓的query parameters 就是从问号后面去分割参数
    std::string argumentStr(start, end);
    std::string::size_type pos = 0;
    std::string::size_type prev = 0;

    // 按照 & 分割多个参数
    while( (pos = argumentStr.find('&', prev))  != std::string::npos )
    {
        std::string pair = argumentStr.substr(prev, pos - prev);
        std::string::size_type equalPos = pair.find('=');

        if(equalPos != std::string::npos)
        {
            std::string key = pair.substr(0, equalPos);
            std::string value = pair.substr(equalPos+1);
            queryParameters_[key] = value;
        }

        prev = pos + 1;
    }

    // 处理最后一个参数
    std::string lastPair = argumentStr.substr(prev);
    std::string::size_type equalPos = lastPair.find('=');
    if(equalPos != std::string::npos)
    {
        // 如果找到了, 就说明有key val
        std::string key = lastPair.substr(0, equalPos);
        std::string value = lastPair.substr(equalPos+1);
        queryParameters_[key] = value;
    }
}

std::string HttpRequest::getQueryParameters(const std::string& key) const
{
    auto it = queryParameters_.find(key);
    if( it != queryParameters_.end())
    {
        return it->second;
    }
    // 没找到我们就返回空
    return "";
}

// 下面这些都是请求头
// ----------------------------------------------------------
// Host: 127.0.0.1:8000
// User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) ...
// Accept: text/html,application/xhtml+xml...
// Connection: keep-alive
//
// ----------------------------------------------------------
void HttpRequest::addHeader(const char* start, const char* colon, const char* end)
{
    std::string key(start, colon);
    ++colon;
    while(colon < end && isspace(*colon))
    {
        // 跳过空白
        ++colon;
    }
    std::string value(colon, end);
    while( !value.empty() && isspace(value[value.size() - 1])) // 消除尾部空格
    {
        value.resize(value.size() - 1);
    }

    headers_[key] = value;
}

std::string HttpRequest::getHeader(const std::string& field) const
{
    std::string result;
    auto it = headers_.find(field);
    if( it != headers_.end())
    {
        return it->second;
    }

    return result;
}

void HttpRequest::swap(HttpRequest& that)
{
    std::swap(method_, that.method_);
    std::swap(path_, that.path_);
    std::swap(pathParameters_, that.pathParameters_);
    std::swap(queryParameters_, that.queryParameters_);
    std::swap(version_, that.version_);
    std::swap(headers_, that.headers_);
    std::swap(receiveTime_, that.receiveTime_);
}

} // namespace http