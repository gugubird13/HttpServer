#pragma once
#include "DbConnection.h"
#include <memory>
#include <queue>
#include <string>
#include <mutex>
#include <condition_variable>
#include <thread>

namespace http
{
namespace db
{

// 单例模式 
class DbConnectionPool
{
public:
    static DbConnectionPool& getInstance()
    {
        // 单例模式，使用 local static
        static DbConnectionPool instance;
        return instance;
    }

    // 初始化连接池
    void init(const std::string& host,
            const std::string& user,
            const std::string& password,
            const std::string& database,
            size_t poolSize = 10);
    
    // 获取连接
    std::shared_ptr<DbConnection> getConnection();

private:
    // 构造函数
    DbConnectionPool();

    // 析构函数
    ~DbConnectionPool();

    // 禁止拷贝
    DbConnectionPool(const DbConnectionPool&) = delete;
    DbConnectionPool& operator=(const DbConnectionPool&) = delete;

    std::shared_ptr<DbConnection> createConnection();

    void checkConnections(); // 添加新的连接检查方法

private:
    std::string                                 host_;
    std::string                                 user_;
    std::string                                 password_;
    std::string                                 database_;
    std::queue<std::shared_ptr<DbConnection>>   connections_;
    std::mutex                                  mutex_;
    std::condition_variable                     cv_;
    bool                                        initialized_ = false;
    std::thread                                 checkThread_; // 检查线程
};

} // namespace db
} // namespace http
