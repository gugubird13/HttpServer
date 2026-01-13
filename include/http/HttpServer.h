#pragma once

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <unordered_map>

#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/base/Logging.h>

#include "HttpContext.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "../router/Router.h"
#include "../session/SessionManager.h"
#include "../middleware/MiddlewareChain.h"
#include "../middleware/cors/CorsMiddleware.h"
#include "../ssl/SslConnection.h"
#include "../ssl/SslContext.h"

/*
 * 底层使用muduo网络库进行构建
 * 关于 SSL 这里的结构是这样的：
 *             HttpServer 启动 -> setSslConfig() -> 创建 SslContext -> 加载 SslConfig 配置
 *             每当有新的连接进来 -> onConnection() -> 创建 SslConnection 对象 -> 进行握手和加密解密
 *             SslConnection 内部持有 TcpConnection 对象的智能指针, SslConnection 使用 SslContext 对象进行 SSL 操作
 *             底层数据接收的时候， 从 TcpConnection 读取加密数据， 通过 SslConnection 解密后交给 HttpServer 处理
 *             发送数据的时候， HttpServer 通过 SslConnection 加密数据， 然后发送到底层 TcpConnection
 *             关于 SSL怎么读到数据的，自然就是 sslconnection 给 Tcpconnection 配置 setMessageCallback方法
 *             这样就能让 ssl 拿到Tcp底层的数据，然后进行处理了，那么自然上层的 onMessage 就是从 ssl 里面拿了
 * 
 * 这里用户拿到这个httpServer，应该自己设置中间件，因为从服务器角度，他并不知道用户想要什么样子的中间件
*/

namespace http
{

class HttpServer : muduo::noncopyable
{
public:
    using HttpCallback = std::function<void (const HttpRequest&, HttpResponse*)>;

    HttpServer(int port,
            const std::string& name,
            const ssl::SslConfig& sslConfig = ssl::SslConfig(),  // <----- 必须传进来
            muduo::net::TcpServer::Option option = muduo::net::TcpServer::kNoReusePort);

    void setThreadNum(int numThreads)
    {
        server_.setThreadNum(numThreads);
    }

    void start();

    muduo::net::EventLoop* getLoop() const{
        return server_.getLoop();
    }

    // 设置HTTP请求的回调函数, 应该是上层使用的接口
    void setHttpCallback(const HttpCallback& cb)
    {
        httpCallback_ = cb;
    }

    // 注册静态路由处理器
    void Get(const std::string& path, const HttpCallback& cb)
    {
        router_.registerCallback(HttpRequest::kGet, path, cb);
    }

    // 注册静态路由处理器
    void Get(const std::string& path, router::Router::HandlerPtr handler)
    {
        router_.registerHandler(HttpRequest::Method::kGet, path, handler);
    }

    void Post(const std::string& path, const HttpCallback& cb)
    {
        router_.registerCallback(HttpRequest::kPost, path, cb);
    }

    void Post(const std::string& path, router::Router::HandlerPtr handler)
    {
        router_.registerHandler(HttpRequest::kPost, path, handler);
    }

    // 注册动态路由处理函数
    void addRoute(HttpRequest::Method method, const std::string& path, router::Router::HandlerPtr handler)
    {
        router_.addRegexHandler(method, path, handler);
    }

    void addRoute(HttpRequest::Method method, const std::string& path, const router::Router::HandlerCallback& cb)
    {
        router_.addRegexCallback(method, path, cb);
    }

    // 设置会话管理
    void setSessionManager(std::unique_ptr<session::SessionManager> manager)
    {
        sessionManager_ = std::move(manager);
    }

    // 获取会话管理器
    session::SessionManager* getSessionManager() const{
        return sessionManager_.get();
    }

    // 添加中间件的方法
    void addMiddleware(std::shared_ptr<middleware::Middleware> middleware)
    {
        middlewareChain_.addMiddleware(middleware);
    }

    void enableSSL(bool enable)
    {
        useSSL_ = enable;
    }

    bool getSslStatus() const { return useSSL_; }

    void setSslConfig(const ssl::SslConfig& config);

private:
    void initialize(const ssl::SslConfig& config);
    void onConnection(const muduo::net::TcpConnectionPtr& conn);
    void onMessage(const muduo::net::TcpConnectionPtr&conn,
                    muduo::net::Buffer* buf,
                    muduo::Timestamp receiveTime);
    void onRequest(const muduo::net::TcpConnectionPtr&, const HttpRequest&);
    void handleRequest(const HttpRequest& req, HttpResponse* resp);

private:
    muduo::net::InetAddress                         listenAddr_;        // 监听地址, 给TcpServer 初始化用的
    muduo::net::TcpServer                           server_;
    muduo::net::EventLoop                           mainLoop_;          // 主循环
    HttpCallback                                    httpCallback_;      // 回调
    router::Router                                  router_;            // 路由
    std::unique_ptr<session::SessionManager>        sessionManager_;    // 路由管理
    middleware::MiddlewareChain                     middlewareChain_;   // 中间链
    std::unique_ptr<ssl::SslContext>                sslCtx_;            // SSL 上下文
    bool                                            useSSL_;            // 是否使用SSL
    // TcpConnectionPtr   ->   SslConnectionPtr
    std::map<muduo::net::TcpConnectionPtr, std::unique_ptr<ssl::SslConnection>> sslConnections_;
};


} // namespace http
