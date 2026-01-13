#include "../include/http/HttpServer.h"
#include "../include/router/Router.h"
#include "../include/middleware/cors/CorsMiddleware.h"
#include "../include/middleware/cors/CorsConfig.h"
#include <muduo/base/Logging.h>
#include <iostream>
#include <sstream>
#include <ctime>

using namespace http;
using namespace http::middleware;

// ==================== 简单的路由处理函数 ====================

// 主页
void handleIndex(const HttpRequest& /* req */, HttpResponse* resp) {
    LOG_INFO << "GET / called";
    std::string body = R"(
<!DOCTYPE html>
<html>
<head>
    <title>HTTP Server Test</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 40px; }
        h1 { color: #333; }
        .api-list { background: #f5f5f5; padding: 15px; border-radius: 5px; }
        code { background: #e0e0e0; padding: 2px 5px; border-radius: 3px; }
    </style>
</head>
<body>
    <h1>🚀 HTTP Server 测试页面</h1>
    <p>这个服务器运行正常！</p>
    <div class="api-list">
        <h2>可用的 API 端点：</h2>
        <ul>
            <li><code>GET /</code> - 主页</li>
            <li><code>GET /api/status</code> - 服务器状态</li>
            <li><code>GET /api/time</code> - 当前服务器时间</li>
            <li><code>POST /api/echo</code> - 回显请求数据</li>
            <li><code>GET /api/users</code> - 获取用户列表</li>
            <li><code>POST /api/users</code> - 创建用户</li>
        </ul>
    </div>
</body>
</html>
    )";
    
    resp->setStatusCode(HttpResponse::k200Ok);
    resp->setContentType("text/html; charset=utf-8");
    resp->setBody(body);
}

// 服务器状态
void handleStatus(const HttpRequest& /* req */, HttpResponse* resp) {
    LOG_INFO << "GET /api/status called";
    std::string body = R"({
    "status": "running",
    "version": "1.0.0",
    "uptime": "just started",
    "message": "HTTP Server is working correctly!"
})";
    
    resp->setStatusCode(HttpResponse::k200Ok);
    resp->setContentType("application/json");
    resp->setBody(body);
}

// 获取当前时间
void handleTime(const HttpRequest& /* req */, HttpResponse* resp) {
    LOG_INFO << "GET /api/time called";
    
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    char buffer[100];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
    
    std::stringstream json;
    json << "{\n"
         << "    \"timestamp\": " << (long)now << ",\n"
         << "    \"datetime\": \"" << buffer << "\",\n"
         << "    \"timezone\": \"Local Time\"\n"
         << "}";
    
    resp->setStatusCode(HttpResponse::k200Ok);
    resp->setContentType("application/json");
    resp->setBody(json.str());
}

// 回显功能
void handleEcho(const HttpRequest& req, HttpResponse* resp) {
    LOG_INFO << "POST /api/echo called";
    
    // 获取请求体
    std::string requestBody = req.getBody();
    
    std::stringstream json;
    json << "{\n"
         << "    \"received\": \"" << requestBody << "\",\n"
         << "    \"method\": \"POST\",\n"
         << "    \"path\": \"" << req.path() << "\",\n"
         << "    \"echoed_at\": \"server received your message\"\n"
         << "}";
    
    resp->setStatusCode(HttpResponse::k200Ok);
    resp->setContentType("application/json");
    resp->setBody(json.str());
}

// 获取用户列表
void handleGetUsers(const HttpRequest& /* req */, HttpResponse* resp) {
    LOG_INFO << "GET /api/users called";
    
    std::string body = R"([
    {
        "id": 1,
        "name": "Alice",
        "email": "alice@example.com"
    },
    {
        "id": 2,
        "name": "Bob",
        "email": "bob@example.com"
    },
    {
        "id": 3,
        "name": "Charlie",
        "email": "charlie@example.com"
    }
])";
    
    resp->setStatusCode(HttpResponse::k200Ok);
    resp->setContentType("application/json");
    resp->setBody(body);
}

// 创建用户
void handleCreateUser(const HttpRequest& /* req */, HttpResponse* resp) {
    LOG_INFO << "POST /api/users called";
    
    std::string body = R"({
    "id": 4,
    "name": "NewUser",
    "email": "newuser@example.com",
    "created": true,
    "message": "User created successfully"
})";
    
    resp->setStatusCode(HttpResponse::k200Ok);
    resp->setContentType("application/json");
    resp->setBody(body);
}

// ==================== 主程序 ====================

int main(int /* argc */, char** /* argv */) {
    // 设置日志级别
    muduo::Logger::setLogLevel(muduo::Logger::INFO);
    
    LOG_INFO << "====================================";
    LOG_INFO << "  HTTP Server - Starting";
    LOG_INFO << "====================================";
    
    try {
        // 1. 创建 HTTP Server（监听 8080 端口）
        ssl::SslConfig sslConfig;
        sslConfig.setCertificateFile("./server.crt");
        sslConfig.setPrivateKeyFile("./server.key");
        // 默认server 构造的 sslConfig 参数是空的，所以如果我们不指定，就是不开启SSL

        // 创建 Server 的时候直接传入配置，会在内部initialize完成所有初始化
        HttpServer server(8080, "TestHttpServer", sslConfig);

        // 2. 配置 CORS（允许跨域请求）
        CorsConfig corsConfig;
        corsConfig.allowedOrigins = {"*"};
        corsConfig.allowedMethods = {"GET", "POST", "PUT", "DELETE", "OPTIONS"};
        corsConfig.allowedHeaders = {"Content-Type", "Authorization"};
        corsConfig.maxAge = 3600;
        
        auto corsMiddleware = std::make_shared<CorsMiddleware>(corsConfig);
        server.addMiddleware(corsMiddleware);
        
        LOG_INFO << "✓ CORS Middleware configured";

        // 3. 注册路由处理函数
        server.Get("/", handleIndex);
        server.Get("/api/status", handleStatus);
        server.Get("/api/time", handleTime);
        server.Post("/api/echo", handleEcho);
        server.Get("/api/users", handleGetUsers);
        server.Post("/api/users", handleCreateUser);
        
        
        LOG_INFO << "✓ Routes registered:";
        LOG_INFO << "  - GET  /";
        LOG_INFO << "  - GET  /api/status";
        LOG_INFO << "  - GET  /api/time";
        LOG_INFO << "  - POST /api/echo";
        LOG_INFO << "  - GET  /api/users";
        LOG_INFO << "  - POST /api/users";
        
        // 4. 启动服务器
        LOG_INFO << "====================================";
        LOG_INFO << "  Server listening on 127.0.0.1:8080";
        LOG_INFO << "====================================";
        LOG_INFO << "Open your browser: http://127.0.0.1:8080/";
        LOG_INFO << "Or use curl:";
        LOG_INFO << "  curl http://127.0.0.1:8080/api/status";
        LOG_INFO << "  curl -X POST http://127.0.0.1:8080/api/echo -d '{\"msg\":\"hello\"}'";
        LOG_INFO << "====================================";
        
        server.start();
        
    } catch (const std::exception& e) {
        LOG_ERROR << "Server error: " << e.what();
        return 1;
    }
    
    return 0;
}
