#pragma once
#include "SslConfig.h"
#include <memory>
#include <muduo/base/noncopyable.h>
#include <openssl/ssl.h>

/*
 *  这个类封装了 SSL_CTX 初始化的时候，会
 *  加载并验证证书文件，设置协议和加密，禁止指定协议的低版本协议
 *  设置会话Cache
 *
*/
namespace ssl {

class SslContext : muduo::noncopyable
{
public:
    explicit SslContext(const SslConfig& config);
    ~SslContext();

    bool initialize();
    SSL_CTX* getNativeHandle() { return ctx_; }

private:
    bool loadCertificates();
    bool setupProtocol();
    void setupSessionCache();
    static void handleSslError(const char* msg);

private:
    SSL_CTX     *ctx_; // SSL 上下文
    SslConfig   config_; // SSL 配置
};

} // namespace ssl
