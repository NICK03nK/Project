#pragma once

#include <list>
#include <mutex>
#include <condition_variable>
#include <thread>

#include "room.hpp"
#include "db.hpp"
#include "online.hpp"
#include "util.hpp"

template<class T>
class match_queue
{
public:
    // 获取元素个数
    int size()
    {
        std::unique_lock<std::mutex> lock(_mtx);

        return _list.size();
    }

    // 判断队列是否为空
    bool empty()
    {
        std::unique_lock<std::mutex> lock(_mtx);

        return _list.empty();
    }

    // 阻塞线程
    void wait()
    {
        std::unique_lock<std::mutex> lock(_mtx);

        _cond.wait(lock);
    }

    // 入队数据，并唤醒线程
    void push(const T& data)
    {
        std::unique_lock<std::mutex> lock(_mtx);

        _list.push_back(data);
        _cond.notify_all();
    }

    // 出队并获取数据
    bool pop(T& data)
    {
        std::unique_lock<std::mutex> lock(_mtx);

        if (_list.empty() == true)
        {
            return false;
        }

        data = _list.front();
        _list.pop_front();

        return true;
    }

    // 移除指定数据
    void remove(const T& data)
    {
        std::unique_lock<std::mutex> lock(_mtx);

        _list.remove(data);
    }

private:
    std::list<T> _list;            // 使用双链表来模拟队列(因为有删除指定元素的需求)
    std::mutex _mtx;               // 互斥锁，实现线程安全
    std::condition_variable _cond; // 条件变量，主要为了阻塞消费者，消费者是从队列中拿数据的，当队列中元素<2时阻塞
};

class matcher
{
public:
    matcher(room_manager* rm, user_table* user_tb, online_manager* user_ol)
        :_rm(rm)
        , _user_tb(user_tb)
        , _user_ol(user_ol)
        , _th_bronze(std::thread(&matcher::thread_bronze_entrance, this))
        , _th_silver(std::thread(&matcher::thread_silver_entrance, this))
        , _th_gold(std::thread(&matcher::thread_gold_entrance, this))
    {
        DLOG("游戏匹配模块初始化完毕");
    }

    // 添加玩家到匹配队列中
    bool add(uint64_t userId)
    {
        // 根据玩家的天梯分数来判定玩家的档次，添加到不同的匹配队列中
        // 1.根据玩家id获取玩家信息
        Json::Value user;
        bool ret = _user_tb->select_by_id(userId, user);
        if (ret == false)
        {
            DLOG("get player: %d information failed", userId);

            return false;
        }

        // 2.将玩家添加到指定队列中
        int score = user["score"].asInt();
        if (score < 2000) // 青铜
        {
            _q_bronze.push(userId);
        }
        else if (score >= 2000 && score <3000) // 白银
        {
            _q_silver.push(userId);
        }
        else // 黄金
        {
            _q_gold.push(userId);
        }

        return true;
    }

    // 将玩家从匹配队列中移除
    bool del(uint64_t userId)
    {
        Json::Value user;
        bool ret = _user_tb->select_by_id(userId, user);
        if (ret == false)
        {
            DLOG("get player: %d information failed", userId);

            return false;
        }

        int score = user["score"].asInt();
        if (score < 2000) // 青铜
        {
            _q_bronze.remove(userId);
        }
        else if (score >= 2000 && score <3000) // 白银
        {
            _q_silver.remove(userId);
        }
        else // 黄金
        {
            _q_gold.remove(userId);
        }

        return true;
    }

private:
    void handler_match(match_queue<uint64_t>& mq)
    {
        while (true)
        {
            // 1.判断匹配队列中玩家个数是否小于2，小于2则阻塞
            while (mq.size() < 2)
            {
                mq.wait();
            }

            // 2.出队两个玩家
            uint64_t userId1 = 0, userId2 = 0;

            bool ret = mq.pop(userId1);
            if (ret == false) // 第一个玩家出队失败
            {
                continue;
            }
            ret = mq.pop(userId2);
            if (ret == false) // 第二个玩家出队失败
            {
                // 执行到这说明第一个玩家出队成功了，第二个玩家出队失败，要先将第一个玩家重新放回匹配队列中
                this->add(userId1);

                continue;
            }

            // 3.检验两个玩家是否在线，如果有玩家掉线，则要将另一个玩家重新放回匹配队列中
            wsserver_t::connection_ptr conn1 = _user_ol->get_conn_from_hall(userId1);
            if (conn1.get() == nullptr) // 玩家1掉线
            {
                this->add(userId2); // 将玩家2重新放回匹配队列中

                continue;
            }
            wsserver_t::connection_ptr conn2 = _user_ol->get_conn_from_hall(userId2);
            if (conn2.get() == nullptr) // 玩家2掉线
            {
                this->add(userId1); // 将玩家1重新放回匹配队列中

                continue;
            }

            // 4.给两个玩家创建游戏房间
            room_ptr rp = _rm->create_room(userId1, userId2);
            if (rp.get() == nullptr)
            {
                // 创建游戏房间失败，将两个玩家重新放入匹配队列中
                this->add(userId1);
                this->add(userId2);

                continue;
            }

            // 5.给两个玩家返回响应
            Json::Value resp;
            resp["optype"] = "match_start";
            resp["result"] = true;

            std::string body;
            json_util::serialize(resp, body);
            conn1->send(body);
            conn2->send(body);
        }
    }

    // 青铜匹配队列的处理线程
    void thread_bronze_entrance() { handler_match(_q_bronze); }

    // 白银匹配队列的处理线程
    void thread_silver_entrance() { handler_match(_q_silver); }

    // 黄金匹配队列的处理线程
    void thread_gold_entrance() { handler_match(_q_gold); }

private:
    match_queue<uint64_t> _q_bronze; // 青铜匹配队列
    match_queue<uint64_t> _q_silver; // 白银匹配队列
    match_queue<uint64_t> _q_gold;   // 黄金匹配队列
    std::thread _th_bronze;          // 青铜线程，用来管理青铜匹配队列的操作
    std::thread _th_silver;          // 白银线程，用来管理白银匹配队列的操作
    std::thread _th_gold;            // 黄金线程，用来管理黄金匹配队列的操作
    room_manager* _rm;               // 房间管理句柄
    user_table* _user_tb;            // 用户数据管理句柄
    online_manager* _user_ol;        // 在线用户管理句柄
};