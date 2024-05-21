#pragma once

#include <list>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <string>

#include "logger.hpp"
#include "room.hpp"
#include "db.hpp"
#include "online.hpp"
#include "util.hpp"

template<class T>
class match_queue
{
public:
    // 获取队列中元素个数
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

    // 阻塞队列所在的线程
    void wait()
    {
        std::unique_lock<std::mutex> lock(_mtx);

        _cond.wait(lock);
    }

    // 将数据入队，并唤醒线程
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
    std::condition_variable _cond; // 条件变量，主要为了阻塞消费者，消费者是从队列中拿数据的，当队列中元素 < 2时阻塞
};

class match_manager
{
public:
    match_manager(room_manager* room_manager, user_table* user_table, online_manager* user_online)
        :_room_manager(room_manager)
        , _user_table(user_table)
        , _user_online(user_online)
        , _bronze_thread(std::thread(&match_manager::bronze_thread_entrance, this))
        , _silver_thread(std::thread(&match_manager::silver_thread_entrance, this))
        , _gold_thread(std::thread(&match_manager::gold_thread_entrance, this))
    {
        DLOG("游戏匹配管理类初始化完毕");
    }

    ~match_manager() { DLOG("游戏匹配管理类即将销毁"); }

    // 将玩家添加到匹配队列中
    bool add(uint64_t user_id)
    {
        // 根据玩家的分数，将玩家添加到不同的匹配队列中
        // 根据玩家id获取玩家信息，读取玩家的分数
        Json::Value user;
        bool ret = _user_table->select_by_id(user_id, user);
        if (ret == false)
        {
            DLOG("获取玩家：%d 的信息失败", user_id);
            return false;
        }

        // 根据分数将玩家加入到对应的匹配队列中
        int score = user["score"].asInt();
        if (score < 2000)
        {
            _bronze_queue.push(user_id);
        }
        else if (score >= 2000 && score < 3000)
        {
            _silver_queue.push(user_id);
        }
        else if (score >= 3000)
        {
            _gold_queue.push(user_id);
        }

        return true;
    }

    // 将玩家从匹配队列中移除
    bool del(uint64_t user_id)
    {
        Json::Value user;
        bool ret = _user_table->select_by_id(user_id, user);
        if (ret == false)
        {
            DLOG("获取玩家：%d 的信息失败", user_id);
            return false;
        }

        int score = user["score"].asInt();
        if (score < 2000)
        {
            _bronze_queue.remove(user_id);
        }
        else if (score >= 2000 && score < 3000)
        {
            _silver_queue.remove(user_id);
        }
        else if (score >= 3000)
        {
            _gold_queue.remove(user_id);
        }

        return true;
    }

private:
    void match_handler(match_queue<uint64_t>& q)
    {
        while (true) // 匹配线程需要一直处于运行状态
        {
            while (q.size() < 2) // 匹配队列中玩家不足两人，阻塞线程
            {
                q.wait();
            }

            // 从匹配队列中出队两个玩家
            uint64_t user_id1 = 0, user_id2 = 0;

            bool ret = q.pop(user_id1);
            if (ret == false) continue; // 第一个玩家出队失败，跳过后续代码重新执行上述代码
            ret = q.pop(user_id2);
            if (ret == false) // 代码执行至此，说明第一个玩家已出队，第二个玩家出队失败，要将出队的第一个玩家重新添加到匹配队列中
            {
                this->add(user_id1);
                continue;
            }

            // 两个玩家都出队成功，则检验两个玩家是否都在游戏大厅，若玩家A掉线，则将玩家B重新添加到匹配队列中
            websocketsvr_t::connection_ptr conn1 = _user_online->get_conn_from_hall(user_id1);
            if (conn1.get() == nullptr)
            {
                this->add(user_id2);
                continue;
            }
            websocketsvr_t::connection_ptr conn2 = _user_online->get_conn_from_hall(user_id2);
            if (conn2.get() == nullptr)
            {
                this->add(user_id1);
                continue;
            }

            // 判断完两个玩家都在线后，给两个玩家创建游戏房间
            room_ptr proom = _room_manager->create_room(user_id1, user_id2);
            if (proom.get() == nullptr) // 创建游戏房间失败
            {
                // 将两个玩家重新放回匹配队列中
                this->add(user_id1);
                this->add(user_id2);
                continue;
            }

            // 给游戏房间内的两个玩家返回响应
            Json::Value resp;
            resp["optype"] = "match_success";
            resp["result"] = true;

            std::string resp_str;
            json_util::serialize(resp, resp_str);
            conn1->send(resp_str);
            conn2->send(resp_str);
        }
    }

    // 青铜匹配队列处理线程
    void bronze_thread_entrance() { match_handler(_bronze_queue); }

    // 白银匹配队列处理线程
    void silver_thread_entrance() { match_handler(_silver_queue); }

    // 黄金匹配队列处理线程
    void gold_thread_entrance() { match_handler(_gold_queue); }

private:
    match_queue<uint64_t> _bronze_queue; // 青铜匹配队列
    match_queue<uint64_t> _silver_queue; // 白银匹配队列
    match_queue<uint64_t> _gold_queue;   // 黄金匹配队列
    std::thread _bronze_thread;          // 青铜线程，用来管理青铜匹配队列的操作
    std::thread _silver_thread;          // 白银线程，用来管理白银匹配队列的操作
    std::thread _gold_thread;            // 黄金线程，用来管理黄金匹配队列的操作
    room_manager* _room_manager;         // 游戏房间管理句柄
    user_table* _user_table;             // 用户数据管理句柄
    online_manager* _user_online;        // 在线用户管理句柄
};