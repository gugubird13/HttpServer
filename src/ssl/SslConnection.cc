#include "../../include/ssl/SslConnection.h"
#include <muduo/base/Logging.h>
#include <openssl/err.h>

namespace ssl
{

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
    LOG_INFO << "SslConnection constructor. ctx pointer: " << ctx 
             << ", native handle: " << (ctx ? ctx->getNativeHandle() : nullptr);
    if(!ssl_)
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

/*
 * 新增功能函数【核心函数】： 数据搬运，把OpenSSL生产的数据搬运到TCP 
 * 就比如:  
 *         1. 底层SSL读到了握手数据，现在握手数据在 writeBIO里面躺着，我们要拿出来，交给Tcp去传输
 *         2. 发送应用数据的时候，ssl默认是发送给了writeBIO就不管了（默认调用SSL_write），但是我们自己清楚这个是virtual的接口，我们要把数据给到Tcp
*/
void SslConnection::sendRetrievedData()
{
    char buf[4096];
    // return the amount of pending data
    int pending = BIO_pending(writeBio_);

    while(pending > 0)
    {
        int bytes = BIO_read(writeBio_, buf, std::min(pending, static_cast<int>(sizeof buf)));
        if(bytes > 0)
        {
            conn_->send(buf, bytes); // 在这里真正发出去了
        }
        pending = BIO_pending(writeBio_);
    }
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

    // 发送应用数据的时候，也需要搬运，所以这里直接用
    sendRetrievedData();
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
        // 【更新】 handleHandShake里面现在会调用 sendRetrievedData，所以这里不管
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

        // 【修复】 即使在已连接状态，OpenSSL 也可能会产生一些协议数据，必须检查并发送
        sendRetrievedData();
    }
}

void SslConnection::handleHandShake()
{
    int ret = SSL_do_handshake(ssl_);

    // 【关键修复】 无论握手是在进行中还是成功，都要检查有没有数据要发送给对方
    // 加上这一行，就不会出现什么 用户发送 hello， 客户端也认为自己发送了hello 而陷入卡死
    sendRetrievedData();

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

// 下面这俩函数属于custom BIO, 不知道什么时候会用上？这里还是先用 memory BIO吧
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

} // namespace ssl
