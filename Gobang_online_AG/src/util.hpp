# pragma once

#include <iostream>
#include <string>
#include <mysql/mysql.h>
#include <jsoncpp/json/json.h>
#include <sstream>
#include <memory>
#include <vector>

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

class json_util
{
public:
    static bool serialize(const Json::Value& root, std::string& str)
    {
        // 1. 实例化一个StreamWriterBuilder工厂类对象
        Json::StreamWriterBuilder swb;

        // 2. 通过StreamWriterBuilder工厂类对象实例化一个StreamWriter对象
        std::unique_ptr<Json::StreamWriter> sw(swb.newStreamWriter());

        // 3. 使用StreamWriter对象，对Json::Value对象中存储的数据进行序列化
        std::stringstream ss;
        int ret = sw->write(root, &ss);
        if (ret != 0)
        {
            ELOG("serialize failed!");
            return false;
        }

        str = ss.str();
        return true;
    }

    static bool unserialize(const std::string& str, Json::Value& root)
    {
        // 1. 实例化一个CharReaderBuilder工厂类对象
        Json::CharReaderBuilder crb;

        // 2. 通过CharReaderBuilder工厂类对象实例化一个CharReader对象
        std::unique_ptr<Json::CharReader> cr(crb.newCharReader());

        // 3. 使用CharReader对象，对str字符串进行Json格式的反序列化
        std::string err;
        bool ret = cr->parse(str.c_str(), str.c_str() + str.size(), &root, &err);
        if (ret == false)
        {
            ELOG("unserialize failed! %s", err.c_str());
            return false;
        }

        return true;
    }
};

class string_util
{
public:
    static int split(const std::string& src, const std::string& sep, std::vector<std::string>& res)
    {
        size_t start = 0, pos = 0;
        while (start < src.size())
        {
            pos = src.find(sep, start);
            if (pos == std::string::npos)
            {
                res.push_back(src.substr(start));
                break;
            }

            if (pos == start)
            {
                start += sep.size();
                continue;
            }

            res.push_back(src.substr(start, pos - start));
            start = pos + sep.size();
        }

        return res.size();
    }
};