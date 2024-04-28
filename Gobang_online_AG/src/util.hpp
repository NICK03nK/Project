# pragma once

#include <iostream>
#include <string>
#include <mysql/mysql.h>

#include "logger.hpp"

class mysql_util
{
public:
    static MYSQL* mysql_create(const std::string& host, const std::string& user, const std::string& passwd, const std::string& dbname, uint32_t port = 3306)
    {
        // 1. 初始化MySQL句柄
        MYSQL *mysql = mysql_init(NULL);
        if (mysql == NULL)
        {
            ELOG("MySQL init failed!");
            return nullptr;
        }

        // 2. 连接服务器
        if (mysql_real_connect(mysql, host.c_str(), user.c_str(), passwd.c_str(), dbname.c_str(), port, NULL, 0) == NULL)
        {
            ELOG("connect MySQL server failed! %s", mysql_error(mysql));
            mysql_close(mysql); // 退出前断开连接，释放mysql操作句柄
            return nullptr;
        }

        // 3. 设置客户端字符集
        if (mysql_set_character_set(mysql, "utf8") != 0)
        {
            ELOG("set client character failed! %s", mysql_error(mysql));
            mysql_close(mysql); // 退出前断开连接，释放mysql操作句柄
            return nullptr;
        }

        return mysql;
    }

    static bool mysql_exec(MYSQL* mysql, const std::string& sql)
    {
        int ret = mysql_query(mysql, sql.c_str());
        if (ret != 0)
        {
            ELOG("%s", sql.c_str());
            ELOG("mysql query failed! %s", mysql_error(mysql));
            mysql_close(mysql); // 退出前断开连接，释放mysql操作句柄
            return false;
        }
    }

    static void mysql_destroy(MYSQL* mysql)
    {
        if (mysql != nullptr) mysql_close(mysql);
    }
};