#include "../../../include/utils/db/DbConnectionPool.h"
#include "../../../include/utils/db/DbException.h"
#include <muduo/base/Logging.h>

namespace http
{
namespace db
{
    
void DbConnectionPool::init(const std::string& host,
        const std::string& user,
        const std::string& password,
        const std::string& database,
        size_t poolSize)
{
    // 连接池会被多个线程访问，所以操作其它成员变量的时候，需要加锁
    std::lock_guard <std::mutex> lock(mutex_);
    // 确保只初始化一次
    if(initialized_)
    {
        return;
    }

    host_ = host;
    user_ = user;
    password_ = password;
    database_ = database;

    // 创建连接
    for(size_t i=0; i<poolSize; ++i)
    {
        connections_.push(createConnection());
    }

    initialized_ = true;
    LOG_INFO << "Database connection pool initialized with " << poolSize << " connections";
}

DbConnectionPool::DbConnectionPool()
{
    checkThread_ = std::thread(&DbConnectionPool::checkConnections, this);
    checkThread_.detach();
}

DbConnectionPool::~DbConnectionPool()
{
    std::lock_guard<std::mutex> lock(mutex_);
    while(!connections_.empty())
    {
        connections_.pop();
    }
    LOG_INFO << "Database connection pool destroyed";
}

// 获取连接
std::shared_ptr<DbConnection> DbConnectionPool::getConnection()
{
    std::shared_ptr <DbConnection> conn;
    {
        std::unique_lock<std::mutex> lock(mutex_);

        while(connections_.empty())
        {
            if(!initialized_)
            {
                throw DbException("Connection pool not initialized");
            }
            LOG_INFO << "Waiting for available connection...";
            cv_.wait(lock);
        }

        conn = connections_.front();
        connections_.pop();
    } // 锁释放

    // 拿到之后，在锁外检查连接
    try
    {
        if(!conn->ping())
        {
            LOG_WARN << "Connection lost, attempting to reconnect...";
            conn->reconnect();
        }

        return std::shared_ptr<DbConnection>(conn.get(), 
            // 这里一定要按照值传递，否则Lambda表达式里面这个就不是局部变量了，那么引用计数就不会变！这个要注意，我们现在想引用计数 +1 的
            [this, conn] (DbConnection*){
                std::lock_guard<std::mutex> lock(mutex_);
                connections_.push(conn);
                cv_.notify_one();
            });
    }
    catch(const std::exception& e)
    // 防止已经拿到 conn之后出错了，那么这个时候就要把它还回去
    {
        LOG_ERROR << "Failed to get connection: " << e.what();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            connections_.push(conn);
            cv_.notify_one();
        }
        // 只是处理了一些逻辑，剩余的锅我不背了，跟编译器说这个错误原封不动抛上去，编译器老哥再往上找找吧!
        // Cathch -> Clean Up -> Re-throw
        throw;
    }
}

std::shared_ptr<DbConnection> DbConnectionPool::createConnection()
{
    return std::make_shared<DbConnection>(host_, user_, password_, database_);
}

// 添加新的连接检查方法
// 守护线程方法，对应的 线程要 detach()
void DbConnectionPool::checkConnections()
{
    while(true)
    {
        try
        {
            std::vector<std::shared_ptr<DbConnection>> connsToCheck;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                if(connections_.empty())
                {
                    // 模拟耗时操作
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    continue;
                }

                // 否则
                auto temp = connections_;
                while(!temp.empty())
                {
                    connsToCheck.push_back(temp.front());
                    temp.pop();
                }
            }

            // 在锁外检查连接
            for(auto& conn : connsToCheck)
            {
                if(!conn->ping())
                {
                    // 如果底层对应的句柄连不上数据库
                    try
                    {
                        conn->reconnect();
                    }
                    catch(const std::exception& e)
                    {
                        LOG_ERROR << "Failed to reconnect: " << e.what();
                    }
                }
            }

            std::this_thread::sleep_for(std::chrono::seconds(60));
        }
        catch(const std::exception& e)
        {
            LOG_ERROR << "Error in check thread: " << e.what();
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
}

} // namespace 
} // namespace http


