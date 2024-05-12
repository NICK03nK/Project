#pragma once

#include <mutex>
#include <cassert>

#include "util.hpp"

class user_table
{
public:
    user_table(const std::string& host, const std::string& user, const std::string& password, const std::string& dbname, uint32_t port = 3306)
    {
        _mysql = mysql_util::mysql_create(host, user, password, dbname, port);
        assert(_mysql != nullptr);
    }

    ~user_table()
    {
        mysql_util::mysql_destroy(_mysql);
        _mysql = nullptr;
    }

    // 注册用户
    bool signup(Json::Value& user)
    {
        // 缺少用户名或密码，注册失败
        if (user["username"].isNull() || user["password"].isNull())
        {
            ELOG("missing username or password!");
            return false;
        }

#define ADD_USER "insert into user values(null, '%s', password('%s'), 1000, 0, 0);"

        char sql[4096] = { 0 };
        sprintf(sql, ADD_USER, user["username"].asCString(), user["password"].asCString());

        bool ret = mysql_util::mysql_exec(_mysql, sql);
        if (ret == false)
        {
            DLOG("add user failed!");
            return false;
        }

        return true;
    }

    // 登录验证，并返回详细的用户信息
    bool login(Json::Value& user)
    {
        // 缺少用户名或密码，登录失败
        if (user["username"].isNull() || user["password"].isNull())
        {
            ELOG("missing username or password!");
            return false;
        }

#define VERIFY_USER "select id, score, total_count, win_count from user where username='%s' and password=password('%s');"

        char sql[4096] = { 0 };
        sprintf(sql, VERIFY_USER, user["username"].asCString(), user["password"].asCString());

        MYSQL_RES* res = nullptr;
        {
            std::unique_lock<std::mutex> lock(_mtx);
            bool ret = mysql_util::mysql_exec(_mysql, sql);
            if (ret == false)
            {
                DLOG("select user information failed!");
                return false;
            }

            // 保存查询结果
            res = mysql_store_result(_mysql);
            int row = mysql_num_rows(res);
            if (res == nullptr || row == 0)
            {
                DLOG("haven't user's information!");
                return false;
            }
        }

        MYSQL_ROW line = mysql_fetch_row(res); // 获取一行查询结果

        // 将用户的详细信息保存到形参user中
        user["id"] = (Json::UInt64)std::stol(line[0]);
        user["score"] = (Json::UInt64)std::stol(line[1]);
        user["total_count"] = std::stoi(line[2]);
        user["win_count"] = std::stoi(line[3]);

        mysql_free_result(res);

        return true;
    }

    // 通过用户名获取详细的用户详细
    bool select_by_username(const std::string& username, Json::Value& user)
    {
#define SELECT_BY_NAME "select id, score, total_count, win_count from user where username='%s';"

        char sql[4096] = { 0 };
        sprintf(sql, SELECT_BY_NAME, username.c_str());

        MYSQL_RES* res = nullptr;
        {
            std::unique_lock<std::mutex> lock(_mtx);
            bool ret = mysql_util::mysql_exec(_mysql, sql);
            if (ret == false)
            {
                DLOG("select user information by username failed!");
                return false;
            }

            // 保存查询结果
            res = mysql_store_result(_mysql);
            int row = mysql_num_rows(res);
            if (res == nullptr || row == 0)
            {
                DLOG("haven't user's information!");
                return false;
            }
        }

        MYSQL_ROW line = mysql_fetch_row(res); // 获取一行查询结果

        // 将用户的详细信息保存到形参user中
        user["id"] = (Json::UInt64)std::stol(line[0]);
        user["username"] = username;
        user["score"] = (Json::UInt64)std::stol(line[1]);
        user["total_count"] = std::stoi(line[2]);
        user["win_count"] = std::stoi(line[3]);

        mysql_free_result(res);

        return true;
    }

    // 通过用户id获取详细的用户详细
    bool select_by_id(uint64_t id, Json::Value& user)
    {
#define SELECT_BY_ID "select username, score, total_count, win_count from user where id=%d;"

        char sql[4096] = { 0 };
        sprintf(sql, SELECT_BY_ID, id);

        MYSQL_RES* res = nullptr;
        {
            std::unique_lock<std::mutex> lock(_mtx);
            bool ret = mysql_util::mysql_exec(_mysql, sql);
            if (ret == false)
            {
                DLOG("select user information by id failed!");
                return false;
            }

            // 保存查询结果
            res = mysql_store_result(_mysql);
            int row = mysql_num_rows(res);
            if (res == nullptr || row == 0)
            {
                DLOG("haven't user's information!");
                return false;
            }
        }

        MYSQL_ROW line = mysql_fetch_row(res); // 获取一行查询结果

        // 将用户的详细信息保存到形参user中
        user["id"] = (Json::UInt64)id;
        user["username"] = line[0];
        user["score"] = std::stoi(line[1]);
        user["total_count"] = std::stoi(line[2]);
        user["win_count"] = std::stoi(line[3]);

        mysql_free_result(res);

        return true;
    }

    // 玩家获胜，分数+30，总场+1，胜场+1
    bool victory(uint64_t id)
    {
        // 根据id查询是否有该玩家
        Json::Value val;
        if (select_by_id(id, val) == false)
        {
            return false;
        }

#define WIN_GAME "update user set score=score+30, total_count=total_count+1, win_count=win_count+1 where id=%d;"

        char sql[4096] = { 0 };
        sprintf(sql, WIN_GAME, id);

        bool ret = mysql_util::mysql_exec(_mysql, sql);
        if (ret == false)
        {
            DLOG("update winner's info failed!");
            return false;
        }

        return true;
    }

    // 玩家失败，分数-30，总场+1，其他不变
    bool defeat(uint64_t id)
    {
        // 根据id查询是否有该玩家
        Json::Value val;
        if (select_by_id(id, val) == false)
        {
            return false;
        }

#define LOSE_GAME "update user set score=score-30, total_count=total_count+1 where id=%d;"

        char sql[4096] = { 0 };
        sprintf(sql, LOSE_GAME, id);

        bool ret = mysql_util::mysql_exec(_mysql, sql);
        if (ret == false)
        {
            DLOG("update loser's info failed!");
            return false;
        }

        return true;
    }

private:
    MYSQL* _mysql; // mysql操作句柄
    std::mutex _mtx; // 互斥锁，保证数据库的访问操作的安全性
};