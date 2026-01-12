#pragma once
#include "db/DbConnectionPool.h"

#include <string>

namespace http
{

// 该类提供了一个便捷的数据库操作接口
class MysqlUtil
{
public:
    static void init(const std::string& host, const std::string& user,
                    const std::string& password, const std::string& database,
                    size_t poolSize = 10)
    {
        http::db::DbConnectionPool::getInstance().init(host, user, password, database, poolSize);
        // 我们发现在这里，init函数被调用了
    }

    template<typename... Args>
    sql::ResultSet* executeQuery(const std::string& sql, Args&&... args)
    {
        auto conn = http::db::DbConnectionPool::getInstance().getConnection();
        return conn->executeQuery(sql, std::forward<Args>(args)...);
    }

    template<typename... Args>
    int executeUpdate(const std::string& sql, Args&&... args)
    {
        auto conn = http::db::DbConnectionPool::getInstance().getConnection();
        return conn->executeUpdate(sql, std::forward<Args>(args)...);
    }

private:

};  

} // namespace http
