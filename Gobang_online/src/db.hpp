#pragma once

#include <mutex>
#include <cassert>
#include <cstdio>

#include "util.hpp"

class user_table
{
public:
    user_table(const std::string& host,
               const std::string& user,
               const std::string& password,
               const std::string& dbname,
               unsigned int port = 3306)
    {
        _mysql = mysql_util::mysql_create(host, user, password, dbname, port);
        assert(_mysql != NULL); // mysql操作句柄为NULL直接退出程序
    }

    // 注册时新增用户
    bool signup(Json::Value& user)
    {
        // 缺少用户名或密码，注册失败
        if (user["username"].isNull() || user["password"].isNull())
        {
            DLOG("missing username or password");

            return false;
        }
        
#define ADD_USER "insert user values(null, '%s', password('%s'), 1000, 0, 0);"

        char sql[4096] = { 0 };
        sprintf(sql, ADD_USER, user["username"].asCString(), user["password"].asCString());

        bool ret = mysql_util::mysql_exec(_mysql, sql);
        if (ret == false)
        {
            DLOG("user sign up failed");

            return false;
        }

        return true;
    }

    // 登录验证，并返回详细的用户信息
    bool signin(Json::Value& user)
    {
        // 缺少用户名或密码，登录失败
        if (user["username"].isNull() || user["password"].isNull())
        {
            DLOG("missing username or password");

            return false;
        }

        // 以用户名和密码共同作为查询过滤条件，查询到数据则表示用户密码一致，为查询到数据则表示用户密码错误
#define VERIFY_USER "select id, score, total_count, win_count from user where username='%s' and password=password('%s');"

        char sql[4096] = { 0 };
        sprintf(sql, VERIFY_USER, user["username"].asCString(), user["password"].asCString());

        MYSQL_RES *res = NULL;
        {
            std::unique_lock<std::mutex> lock(_mtx); // 对于mysql_exec()和mysql_store_result()的线程安全加锁保护

            bool ret = mysql_util::mysql_exec(_mysql, sql);
            if (ret == false)
            {
                DLOG("user sign in failed");

                return false;
            }

            // 将查询结果保存到本地
            res = mysql_store_result(_mysql);
            if (res == NULL)
            {
                // 表中没有该数据，查询失败
                DLOG("sign in failed, wrong username or password");

                return false;
            }
        }

        // 代码执行到这，说明表中存在该登录用户，且用户名和密码对的上
        uint64_t row_num = mysql_num_rows(res);

        // 由于username设置了unique key属性，所以查询的结果只能有一条
        if (row_num != 1)
        {
            DLOG("the user's information is not unique");

            return false;
        }

        MYSQL_ROW row = mysql_fetch_row(res);
        user["id"] = (Json::UInt64)std::stol(row[0]);
        user["score"] = (Json::UInt64)std::stol(row[1]);
        user["total_count"] = std::stoi(row[2]);
        user["win_count"] = std::stoi(row[3]);

        mysql_free_result(res);

        return true;
    }

    // 通过用户名获取用户信息
    bool select_by_name(const std::string& name, Json::Value& user)
    {
#define SELECT_BY_NAME "select id, score, total_count, win_count from user where username='%s';"

        char sql[4096] = { 0 };
        sprintf(sql, SELECT_BY_NAME, name.c_str());

        MYSQL_RES *res = NULL;
        {
            std::unique_lock<std::mutex> lock(_mtx); // 对于mysql_exec()和mysql_store_result()的线程安全加锁保护

            bool ret = mysql_util::mysql_exec(_mysql, sql);
            if (ret == false)
            {
                DLOG("get user by name failed");

                return false;
            }

            // 将查询结果保存到本地
            res = mysql_store_result(_mysql);
            if (res == NULL)
            {
                // 表中没有该数据，查询失败
                DLOG("have no user's information");

                return false;
            }
        }

        // 代码执行到这，说明表中存在该登录用户，且用户名和密码对的上
        uint64_t row_num = mysql_num_rows(res);

        // 由于username设置了unique key属性，所以查询的结果只能有一条
        if (row_num != 1)
        {
            DLOG("the user's information is not unique");

            return false;
        }

        MYSQL_ROW row = mysql_fetch_row(res);
        user["id"] = (Json::UInt64)std::stol(row[0]);
        user["username"] = name;
        user["score"] = (Json::UInt64)std::stol(row[1]);
        user["total_count"] = std::stoi(row[2]);
        user["win_count"] = std::stoi(row[3]);

        mysql_free_result(res);

        return true;
    }

    // 通过id获取用户信息
    bool select_by_id(uint64_t id, Json::Value& user)
    {
#define SELECT_BY_ID "select username, score, total_count, win_count from user where id=%d;"

        char sql[4096] = { 0 };
        sprintf(sql, SELECT_BY_ID, id);

        MYSQL_RES *res = NULL;
        {
            std::unique_lock<std::mutex> lock(_mtx); // 对于mysql_exec()和mysql_store_result()的线程安全加锁保护

            bool ret = mysql_util::mysql_exec(_mysql, sql);
            if (ret == false)
            {
                DLOG("get user by id failed");

                return false;
            }

            // 将查询结果保存到本地
            res = mysql_store_result(_mysql);
            if (res == NULL)
            {
                // 表中没有该数据，查询失败
                DLOG("have no user's information");

                return false;
            }
        }

        // 代码执行到这，说明表中存在该登录用户，且用户名和密码对的上
        uint64_t row_num = mysql_num_rows(res);

        // 由于username设置了unique key属性，所以查询的结果只能有一条
        if (row_num != 1)
        {
            DLOG("the user's information is not unique");

            return false;
        }

        MYSQL_ROW row = mysql_fetch_row(res);
        user["id"] = (Json::UInt64)id;
        user["username"] = row[0];
        user["score"] = (Json::UInt64)std::stol(row[1]);
        user["total_count"] = std::stoi(row[2]);
        user["win_count"] = std::stoi(row[3]);

        mysql_free_result(res);

        return true;
    }

    // 胜利时天梯分数加30分，游戏场数加1，胜场增加1
    bool victory(uint64_t id)
    {
#define WIN_GAME "update user set score=score+30, total_count=total_count+1, win_count=win_count+1 where id=%d;"

        char sql[4096] = { 0 };
        sprintf(sql, WIN_GAME, id);

        bool ret = mysql_util::mysql_exec(_mysql, sql);
        if (ret == false)
        {
            DLOG("update winner's information failed");

            return false;
        }

        return true;
    }

    // 失败时天梯分数减30分，游戏场数加1，其他不变
    bool defeat(uint64_t id)
    {
#define LOSE_GAME "update user set score=score-30, total_count=total_count+1 where id=%d;"

        char sql[4096] = { 0 };
        sprintf(sql, LOSE_GAME, id);

        bool ret = mysql_util::mysql_exec(_mysql, sql);
        if (ret == false)
        {
            DLOG("update loser's information failed");

            return false;
        }

        return true;
    }

    ~user_table()
    {
        mysql_util::mysql_destroy(_mysql);
        _mysql = NULL;
    }

private:
    MYSQL* _mysql;   // mysql操作句柄
    std::mutex _mtx; // 互斥锁，用于保护数据库的访问操作
};