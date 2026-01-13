#pragma once
#include "SslContext.h"
#include <muduo/net/TcpConnection.h>
#include <muduo/net/Buffer.h>
#include <muduo/base/noncopyable.h>
#include <openssl/ssl.h>
#include <memory>

// 我们看到这个类既然已经包含了 TcpConnection 了，说明连接后是想干一些事情的
// 所以这里必然会有回调函数的定义

/*
 *  实际上我们可以把这里这样理解：原本如果没用 bio， 我们的SSL对象直接和 底层的 socket 绑定
 *  但是如果我SSL对象读取对象，我底层并没有用socket 这样的实现怎么办呢？
 *  因此我们要虚构一个 IO，让SSL误以为 ”我确实是 在直接和 底层网络缓冲区直接交互，代码没问题“
 *  那么这样解耦出来，我们就可以用更高性能的库，来传输数据，然后把 数据搬到 BIO 缓冲区这样就可以了
 *  因此我们要用 BIO，把 它和 底层网络解耦，我只管从一个抽象的BIO读取
 *  原本是这样的：      底层socket -> SSL
 *  解耦之后就是这样的：    muduo缓冲区 -> BIO 内存buffer -> SSL_read (从 BIO buffer 读取内容)
 *                                              [前提是SSL_set_bio(ssl_, readBio_, writeBio_); 我们指定了具体 BIO buffer 供读取和写入]
*/

namespace ssl
{

// 添加消息回调函数类型定义
using MessageCallback = std::function<void(const std::shared_ptr<muduo::net::TcpConnection>&,
                                        muduo::net::Buffer*,
                                        muduo::Timestamp)>;

class SslConnection : muduo::noncopyable
{
public:
    // C++ 11 的定义方式去定义在当前作用域
    using TcpConnectionPtr = std::shared_ptr<muduo::net::TcpConnection>;
    using BufferPtr = muduo::net::Buffer*;

    SslConnection(const TcpConnectionPtr& conn, SslContext* ctx);
    ~SslConnection();

    void startHandshake();
    void send(const void* data, size_t len);
    void onRead(const TcpConnectionPtr& conn, BufferPtr buf, muduo::Timestamp time);
    bool isHandshakeComplete() const { return state_ == SSLState::ESTABLISHED; }
    muduo::net::Buffer* getDecryptedBuffer() { return &decryptedBuffer_; }

    // SSL BIO 操作
    static int bioWrite(BIO* bio, const char* data, int len);
    static int bioRead(BIO* bio, char* data, int len);
    static long bioCtrl(BIO* bio, int cmd, long num, void* ptr);

    // 设置消息回调函数
    void setMessageCallback(const MessageCallback& cb) { messageCallback_ = cb; }

private:
    void handleHandShake();
    void sendRetrievedData();
    void onEncrypted(const char* data, size_t len);
    void onDecrypted(const char* data, size_t len);
    SSLError getLastError(int ret);
    void handleError(SSLError error);

private:
    SSL*                            ssl_;               // SSL 连接
    SslContext*                     ctx_;               // SSL 上下文
    TcpConnectionPtr                conn_;              // TCP 连接
    SSLState                        state_;             // SSL 状态
    BIO*                            readBio_;           // 网络数据 -> SSL
    BIO*                            writeBio_;          // SSL -> 网络数据
    muduo::net::Buffer              readBuffer_;        // 读缓冲区
    muduo::net::Buffer              writeBuffer_;       // 写缓冲区
    muduo::net::Buffer              decryptedBuffer_;   // 解密后的数据
    MessageCallback                 messageCallback_;   // 消息回调
};

} // namespace ssl
