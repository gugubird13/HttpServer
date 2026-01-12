#include "../../include/http/HttpServer.h"

#include <any>
#include <functional>
#include <memory>

namespace http
{

HttpServer::HttpServer(int port,
        const std::string& name,
        bool useSSL = false,
        muduo::net::TcpServer::Option option = muduo::net::TcpServer::kNoReusePort)
    : listenAddr_(port)
    , server_(&mainLoop_, listenAddr_, name, option)
    , useSSL_(useSSL)
    , httpCallback_(std::bind(&HttpServer::handleRequest, this, std::placeholders::_1, std::placeholders::_2))
{
    initialize();
}

// 服务器运行函数
void HttpServer::start()
{
    LOG_WARN << "HttpServer[" << server_.name() << "] starts listening on" << server_.ipPort();
    server_.start();            // 设置acceptor, 开启线程池，并在mainLoop 中run in loop 开始监听，并且设置channel 对读事件感兴趣
    mainLoop_.loop();           // mainLoop 开启其下面的 Poller wait 在对应的 channel上
}

void HttpServer::initialize()
{
    // 在这里主要设置两个回调函数
    server_.setConnectionCallback(std::bind(&HttpServer::onConnection, this, std::placeholders::_1));
    // muduo 底层要求信息回调是接受三个参数， 所以我们使用 std::bind 包装成3个参数的函数包装器
    server_.setMessageCallback(std::bind(&HttpServer::onMessage, this,
                            std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void HttpServer::setSslConfig(const ssl::SslConfig& config)
{
    if(useSSL_)
    {
        sslCtx_ = std::make_unique<ssl::SslContext>(config);
        if(!sslCtx_->initialize()) // 看 ssl 上下文有没有初始化成功
        {
            LOG_ERROR << "Failed to initialized SSL context";
            abort();  // 终止程序
        }
    }
}

void HttpServer::onConnection(const muduo::net::TcpConnectionPtr& conn)
{
    // 设置onConnection
    if(conn->connected())
    {
        if(useSSL_)
        {
            // 如果使用的是SSL，我们就创建context
            // 每一个连接都有一个自己的专属的 SslConnection 对象
            auto sslConn = std::make_unique<ssl::SslConnection>(conn, sslCtx_.get());
            // 之前没有ssl的时候，这个 onConnection 只是简单的打印了一下连接信息，现在要把 sslConn 和 conn 关联起来
            // 那么这里就要为ssl配置回调，这个回调的作用就是，当 sslConn 解密出数据后，传递给 HttpServer 进行处理
            // 如果以前没有使用 ssl 的话，简单就是HttpServer 的 onMessage 直接处理就行了, 上报简单也就是
            // 用 TcpConnection 拿到的数据上报就行

            // 给它设置完信息回调，实际上就是在告诉我们，sslConn 要 1. 他要劫持Tcpconn底层的数据(sslConn构造函数自动劫持); 2. 他要代替TcpConn给
            // HttpServer 传送数据
            sslConn->setMessageCallback(
                std::bind(&HttpServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)
            );
            sslConnections_[conn] = std::move(sslConn);
            // 开始握手，一开始我还不理解，为什么connection执行握手？ 找了找connection的头文件， 发现哪有 对端地址啊
            // 然后我才幡然醒悟：实际上握手是建立在TcpConnection上面的，也就是说，Tcp底层建立连接了，
            // 这个握手是基于 tcpconnection的。

            // 这个自动就会执行握手协议
            // 也就是说，我们执行 onConnection是不会触发握手的，只有当第一个hello到达的时候，才会执行握手协议，所以
            // 这个握手会发生在 onRead 的方法之下，因此这里不需要设置
            // sslConnections_[conn]->startHandshake();
        }
        // 这个不管是不是 ssl 都得加上，给这个conn 加一个 context
        // 存一个状态解析器，也就是 context 用来解析请求变成 httpRequest 的
        // 这个必须存的原因在于：如果一个请求过于长了，我们希望实现持久化，也就是我们不希望
        // 解析了一般，然后这个local 变量被销毁了，下次onMessage回调还是创建新的 context，所以我们
        // 就在创建连接的时候，把它放到 TcpConn 的context里面实现持久化
        // 下次 onMessage 的时候，拿出来继续parse就是了
        conn->setContext(HttpContext());
    }
    else
    {
        if(useSSL_)
        {
            sslConnections_.erase(conn);
        }
    }
}

// 这里依然是 onMessage 给底层的TcpServer设置的，然后底层的也就是Channel会执行回调，我们只需要处理数据就行了
// 假设设置了 SSL, 那么具体的就是：
// TcpConnection 处理回调（在ssl里面定义了，先解密，再执行HttpServer给 ssl的回调，也就是下面这个函数）
// 因此下面这个函数，拿到的就是明文了，只需要处理明文就行了
// 不需要像原来那个仓库里面写的那样：先他妈拿到数据，再去调用 底层ssl的相关函数，那不乱套了吗？？？？
// 只能说这个项目原来的代码，很他妈shit，不清楚各个模块的职责，明明设置了回调，还要自己找自己的函数，逻辑混乱！！！！！！
void HttpServer::onMessage(const muduo::net::TcpConnectionPtr&conn,
                muduo::net::Buffer* buf,
                muduo::Timestamp receiveTime)
{
    // 【删除】所有关于 useSSL_ 的判断、find sslConns、手动调用 onRead 的代码全部删掉！
    // 因为这部分工作已经由 SslConnection::onRead 在底层做完了，并回调到了这里。
    HttpContext *context = boost::any_cast<HttpContext>(conn->getMutableContext());

    // 直接解析，这里的buf已经是明文了！！！！
    if(!context->parseRequest(buf, receiveTime));
    {
        conn->send("HTTP/1.1 400 Bad Request\r\n\r\n");
        conn->shutdown();
    }

    if(context->gotAll())
    {
        // 拿到request 之后，直接去处理request了
        onRequest(conn, context->request());
        context->reset();
    }
}

void HttpServer::onRequest(const muduo::net::TcpConnectionPtr& conn, const HttpRequest& req)
{
    // 1. 构造response 需要 close 看是保持连接，还是短连接
    const std::string &connection = req.getHeader("Connection");
    bool close = ((connection == "close")  || (req.getVersion() == "HTTP/1.0" && connection != "Keep-Alive"));

    HttpResponse response(close);

    // 2. 根据请求报文信息，封装响应报文对象
    httpCallback_(req, &response);   // 执行onHttpCallback 函数

    // 可以给response 设置一个成员，判断是否请求的是文件，如果是则设置为true，并且存在文件位置在这里send出去
    muduo::net::Buffer buf;
    // 序列化输出到buf里面
    response.appendToBuffer(&buf);
    // 打印完整的内容响应用于调试
    LOG_INFO << "Sending response:\n" << buf.toStringPiece().as_string();

    conn->send(&buf);
    // 如果是短链接，返回响应报文之后就断开
    if(response.closeConnection())
    {
        conn->shutdown();
    }
}

void HttpServer::handleRequest(const HttpRequest& req, HttpResponse* resp)
{
    try
    {
        // 处理请求前的中间件
        HttpRequest mutableReq = req;
        middlewareChain_.processBefore(mutableReq);

        // 路由处理
        if(!router_.route(mutableReq, resp))
        {
            LOG_INFO << "请求的URL: " << req.method() << " " << req.path();
            LOG_INFO << "未找到路由, 返回404";
            resp->setStatusCode(HttpResponse::k404NotFound);
            resp->setStatusMessage("Not Found");
            resp->setCloseConnection(true);
        }

        // 处理响应后的中间件
        middlewareChain_.processAfter(*resp);
    }
    catch (const HttpResponse& res)
    {
        // 处理中间件抛出的响应( CorsMiddleware.cc 代码27行)
        *resp = res;
    }
    catch(const std::exception& e)
    {
        // 错误处理
        resp->setStatusCode(HttpResponse::k500InternalServerError);
        resp->setBody(e.what());
    }
}

} // namespace http
