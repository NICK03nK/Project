#pragma once

#include <string>
#include <mysql/mysql.h>
#include <jsoncpp/json/json.h>
#include <memory>
#include <sstream>

#include "logger.hpp"

// mysql工具类
class mysql_util
{
public:
    // 创建mysql句柄并完成初始化
    static MYSQL* mysql_create(const std::string &host,
                               const std::string &user,
                               const std::string &password,
                               const std::string &dbname,
                               unsigned int port = 3306)
    {
        // 初始化mysql句柄
        MYSQL *mysql = mysql_init(NULL);
        if (mysql == NULL)
        {
            ELOG("mysql init failed");

            return nullptr;
        }

        // 连接mysql服务器
        if (mysql_real_connect(mysql, host.c_str(), user.c_str(), password.c_str(), dbname.c_str(), port, NULL, 0) == NULL)
        {
            ELOG("connect mysql server failed: %s", mysql_error(mysql));
            mysql_close(mysql); // 出错退出前要断开连接销毁句柄

            return nullptr;
        }

        // 设置客户端字符集
        if (mysql_set_character_set(mysql, "utf8") != 0)
        {
            ELOG("set client character failed: %s", mysql_error(mysql));
            mysql_close(mysql); // 出错退出前要断开连接销毁句柄

            return nullptr;
        }

        return mysql;
    }

    // 执行sql语句
    static bool mysql_exec(MYSQL* mysql, const std::string& sql)
    {
        // 执行sql语句
        int ret = mysql_query(mysql, sql.c_str());
        if (ret != 0)
        {
            ELOG("%s", sql.c_str());
            ELOG("mysql query failed: %s", mysql_error(mysql));
            mysql_close(mysql); // 出错退出前要断开连接销毁句柄

            return false;
        }

        return true;
    }

    // 销毁mysql句柄
    static void mysql_destroy(MYSQL* mysql)
    {
        if (mysql != nullptr)
        {
            mysql_close(mysql);
        }
    }
};

// json工具类
class json_util
{
public:
    // 序列化
    static bool serialize(const Json::Value& root, std::string& str)
    {
        // 实例化一个StreamWriterBuilder工厂类对象
        Json::StreamWriterBuilder swb;

        // 通过StreamWriterBuilder工厂类对象生产一个StreamWriter对象
        std::unique_ptr<Json::StreamWriter> sw(swb.newStreamWriter());

        // 使用StreamWriter对象，对Json::Value中的数据进行序列化，将序列化数据写入ss中
        std::stringstream ss;
        int ret = sw->write(root, &ss);
        if (ret != 0)
        {
            ELOG("json serialize failed");

            return false;
        }
        
        str = ss.str(); // 将序列化数据赋给str
        return true;
    }

    // 反序列化
    static bool unserialize(const std::string& str, Json::Value& root)
    {
        // 实例化一个CharReaderBuilder工厂类对象
        Json::CharReaderBuilder crb;

        // 通过CharReaderBuilder工厂类对象生产一个CharReader对象
        std::unique_ptr<Json::CharReader> cr(crb.newCharReader());

        // 使用CharReader对象，对json格式的字符串进行反序列化
        std::string errs;
        bool ret = cr->parse(str.c_str(), str.c_str() + str.size(), &root, &errs);
        if (ret == false)
        {
            ELOG("json unserialize failed: %s", errs.c_str());

            return false;
        }

        return true;
    }
};