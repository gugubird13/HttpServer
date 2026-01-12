#include "../../include/ssl/SslConnection.h"
#include <muduo/base/Logging.h>
#include <openssl/err.h>

namespace ssl
{

// 自定义 BIO 方法
static BIO_METHOD* createCustomBioMethod()
{
    BIO_METHOD* method = BIO_meth_new(BIO_TYPE_MEM, "custom");
    BIO_meth_set_write(method, SslConnection::bioWrite);
    BIO_meth_set_read(method, SslConnection::bioRead);
    BIO_meth_set_ctrl(method, SslConnection::bioCtrl);
    return method;
}

SslConnection::SslConnection(const TcpConnectionPtr& conn, SslContext* ctx)
    : ssl_(nullptr)
    , ctx_(ctx)
    , conn_(conn)
    , state_(SSLState::HANDSHAKE)
    , readBio_(nullptr)
    , writeBio_(nullptr)
    , messageCallback_(nullptr)
{
    // 创建 SSL 对象
    ssl_ = SSL_new(ctx_->getNativeHandle());
    if(ssl_)
    {
        // ERR_get_error() returns the earliest error code from the thread's error queue and removes the
        // entry. 再利用 ERR_error_string 转换 error code 变成 hunman-readable 的string
        LOG_ERROR << "Failed to create SSL object: " << ERR_error_string(ERR_get_error(), nullptr);
        return;
    }

    // 创建BIO
    // 创建内存式的BIO，脱离了网络式的复杂构造，我只需要在内存里面拿到就行了，其它的我不管
    // 怎么发送让程序员自己决定吧, 从死板的 socket 范式变成 我想用什么方法都行，比如我想发到数据库
    // 比如我想用epoll来实现
    readBio_ = BIO_new(BIO_s_mem());
    writeBio_ = BIO_new(BIO_s_mem());

    if(!readBio_ || !writeBio_)
    {
        LOG_ERROR << "Failed to create BIO objects";
        SSL_free(ssl_);
        ssl_ = nullptr;
        return;
    }

    SSL_set_bio(ssl_, readBio_, writeBio_);
    SSL_set_accept_state(ssl_);   // 设置为服务器模式

    // 设置 SSL 选项
    SSL_set_mode(ssl_, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
    SSL_set_mode(ssl_, SSL_MODE_ENABLE_PARTIAL_WRITE);

    // 设置连接回调
    conn_->setMessageCallback(std::bind(&SslConnection::onRead, this, std::placeholders::_1,
                                std::placeholders::_2, std::placeholders::_3));
}

SslConnection::~SslConnection()
{
    if(ssl_)
    {
        SSL_free(ssl_);   // 这回同时释放 BIO
    }
}

void SslConnection::startHandshake()
{
    handleHandShake();
}

void SslConnection::send(const void* data, size_t len)
{
    if(state_ != SSLState::ESTABLISHED)
    {
        LOG_ERROR << "Cannot send data before SSL handshake is complete";
        return;
    }

    int written = SSL_write(ssl_, data, len);
    if(written < 0)
    {
        int err = SSL_get_error(ssl_, written);
        LOG_ERROR << "SSL_write failed" << ERR_error_string(err, nullptr);
        return;
    }

    char buf[4096];
    int pending;
    while((pending = BIO_pending(writeBio_)) > 0){
        int bytes = BIO_read(writeBio_, buf, std::min(pending, static_cast<int>(sizeof buf)));
        if(bytes > 0)
        {
            conn_->send(buf, bytes);
        }
    }
}

void SslConnection::onRead(const TcpConnectionPtr& conn, BufferPtr buf, muduo::Timestamp time)
{
    // ==========================================================
    // 步骤 1：先把网络收到的所有“生肉”（加密数据）统统喂给 OpenSSL 的 BIO
    // ==========================================================
    if (buf->readableBytes() > 0) {
        // 把数据写入 readBio
        int written = BIO_write(readBio_, buf->peek(), buf->readableBytes());

        if (written > 0) {
            // 从网络缓冲区中移除已写入 BIO 的数据
            buf->retrieve(written);
        } else {
            // BIO 写失败处理（极少见，除非内存炸了）
            LOG_ERROR << "BIO_write failed";
            return;
        }
    }

    // ==========================================================
    // 步骤 2：根据状态机驱动 SSL 处理
    // ==========================================================
    if (state_ == SSLState::HANDSHAKE) {
        // 握手阶段：SSL 引擎会从 readBio 里读取握手包进行处理
        handleHandShake();

    } else if (state_ == SSLState::ESTABLISHED) {
        // 通信阶段：循环解密数据
        char decryptedData[4096];
        muduo::net::Buffer decryptedBuffer;
        bool hasData = false;

        while (true) {
            // SSL_read 会自动从 readBio_ 里取数据进行解密
            int ret = SSL_read(ssl_, decryptedData, sizeof(decryptedData));

            if (ret > 0) {
                // 读到了解密后的数据
                decryptedBuffer.append(decryptedData, ret);
                hasData = true;
            } else {
                // ret <= 0 说明 BIO 里没有完整的应用层数据包了，或者出错了
                int err = SSL_get_error(ssl_, ret);
                if (err == SSL_ERROR_WANT_READ) {
                    // 正常情况：数据读完了，等待下一次网络数据
                    break;
                } else if (err == SSL_ERROR_ZERO_RETURN) {
                    // 对端关闭了 SSL 连接
                    // handleClose();
                    break;
                } else {
                    // 真的出错了
                    LOG_ERROR << "SSL_read error: " << err;
                    break;
                }
            }
        }

        // 如果解密出了数据，回调给用户
        if (hasData && messageCallback_) {
            messageCallback_(conn, &decryptedBuffer, time);
        }
    }
}

void SslConnection::handleHandShake()
{
    int ret = SSL_do_handshake(ssl_);

    if(ret == 1)
    {
        state_ = SSLState::ESTABLISHED;
        LOG_INFO << "SSL handshake completed successfully";
        LOG_INFO << "Using cipher: " << SSL_get_cipher(ssl_);  // 加密套文
        LOG_INFO << "Protocol version: " << SSL_get_version(ssl_);

        // 握手完成了，还要确保设置了正确的回调函数，因为这个也要用上层给它的回调函数，处理一些东西往上传
        if(!messageCallback_)
        {
            LOG_WARN << "No message callback set after SSL handshake";
        }
        return;
    }

    // 否则找错误
    int err = SSL_get_error(ssl_, ret);
    switch(err)
    {
        case SSL_ERROR_WANT_READ:
        case SSL_ERROR_WANT_WRITE:
            // 正常的握手过程，需要继续
            break;

        default:{
            // 获取详细的错误信息
            char errBuf[256];
            unsigned long errCode = ERR_get_error();
            ERR_error_string_n(errCode, errBuf, sizeof errBuf);
            LOG_ERROR << "SSL handshake failed: " << errBuf;
            conn_->shutdown(); // 关闭连接
            break;
        }
    }
}

void SslConnection::onEncrypted(const char* data, size_t len)
{
    writeBuffer_.append(data, len);
    conn_->send(&writeBuffer_);
}

void SslConnection::onDecrypted(const char* data, size_t len)
{
    decryptedBuffer_.append(data, len);
}

SSLError SslConnection::getLastError(int ret)
{
    int err = SSL_get_error(ssl_, ret);
    switch (err)
    {
        case SSL_ERROR_NONE:
            return SSLError::NONE;
        case SSL_ERROR_WANT_READ:
            return SSLError::WANT_READ;
        case SSL_ERROR_WANT_WRITE:
            return SSLError::WANT_WRITE;
        case SSL_ERROR_SYSCALL:
            return SSLError::SYSCALL;
        case SSL_ERROR_SSL:
            return SSLError::SSL;
        default:
            return SSLError::UNKNOWN;
    }
}

void SslConnection::handleError(SSLError error)
{
    switch (error)
    {
        case SSLError::WANT_READ:
        case SSLError::WANT_WRITE:
            // 需要等待更多数据或写入缓冲区可用
            break;
        case SSLError::SSL:
        case SSLError::SYSCALL:
        case SSLError::UNKNOWN:
            LOG_ERROR << "SSL error occurred: " << ERR_error_string(ERR_get_error(), nullptr);
            state_ = SSLState::ERROR;
            conn_->shutdown();
            break;
        default:
            break;
    }
}

int SslConnection::bioWrite(BIO* bio, const char* data, int len)
{
    // 这个函数返回的值是void * 的，也就是返回的是自定义的数据类型，和用户自己 custom 的 BIO有关
    SslConnection* conn = static_cast<SslConnection*>(BIO_get_data(bio));
    if(!conn) return -1;

    conn->conn_->send(data, len);
    return len;
}

int SslConnection::bioRead(BIO* bio, char* data, int len)
{
    SslConnection* conn = static_cast<SslConnection*> (BIO_get_data(bio));
    if(!conn) return -1;

    size_t readable = conn->readBuffer_.readableBytes();
    if(readable == 0)
    {
        return -1; // 无数据可以读
    }

    size_t toRead = std::min(static_cast<size_t>(len), readable);
    memcpy(data, conn->readBuffer_.peek(), toRead);
    conn->readBuffer_.retrieve(toRead);
    return toRead;
}

long SslConnection::bioCtrl(BIO* bio, int cmd, long num, void* ptr)
{
    switch(cmd)
    {
        case BIO_CTRL_FLUSH:
            return 1;
        default:
            return 0;
    }
}


} // namespace ssl
