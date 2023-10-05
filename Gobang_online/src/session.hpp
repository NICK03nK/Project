#pragma once

#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>
#include <mutex>
#include <unordered_map>

#include "logger.hpp"

using wsserver_t = websocketpp::server<websocketpp::config::asio>;

enum ss_status
{
    LOGIN,
    UNLOGIN
};

class session
{
public:
    session(uint64_t ssid)
        :_ssid(ssid)
    {
        DLOG("SESSION %p is created", this);
    }

    uint64_t ssid() { return _ssid; }

    void set_status(ss_status status) { _status = status; }

    void set_user(uint64_t userId) { _userId = userId; }

    uint64_t get_user() { return _userId; }

    void set_timer(const wsserver_t::timer_ptr &tp) { _tp = tp; }

    wsserver_t::timer_ptr get_timer() { return _tp; }

    bool is_login() { return _status == LOGIN; }

    ~session()
    {
        DLOG("SESSION %p is released", this);
    }

private:
    uint64_t _ssid;            // session id
    ss_status _status;         // 用户的状态信息(登录/未登录)
    uint64_t _userId;          // 用户id
    wsserver_t::timer_ptr _tp; // session关联的定时器
};

#define SESSION_TEMOPRARY 30000
#define SESSION_PERMANENT -1

using session_ptr = std::shared_ptr<session>;
class session_manager
{
public:
    session_manager(wsserver_t* svr)
        :_next_ssid(1)
        , _server(svr)
    {
        DLOG("session管理模块初始化完毕");
    }

    // 创建session
    session_ptr create_session(uint64_t userId, ss_status status)
    {
        std::unique_lock<std::mutex> lock(_mtx);

        session_ptr ssp(new session(_next_ssid));
        ssp->set_status(status);
        ssp->set_user(userId);

        _ssid_session.insert(std::make_pair(_next_ssid, ssp));
        ++_next_ssid;

        return ssp;
    }

    // 向_ssid_session重新添加一个已存在的session
    void append_session(const session_ptr& ssp)
    {
        std::unique_lock<std::mutex> lock(_mtx);

        _ssid_session.insert(std::make_pair(ssp->ssid(), ssp));
    }

    // 通过sessin id获取session
    session_ptr get_session_by_ssid(uint64_t ssid)
    {
        std::unique_lock<std::mutex> lock(_mtx);

        auto it = _ssid_session.find(ssid);
        if (it == _ssid_session.end())
        {
            DLOG("no session with session id: %lu", ssid);
            
            return session_ptr();
        }

        return it->second;
    }

    // 设置session过期时间
    void set_session_expiration_time(uint64_t ssid, int ms)
    {
        // 依赖于websocketpp的定时器来实现对session生命周期的管理

        // 用户登录后，创建session，此时的session是临时的，在指定时间内无通信就被删除
        // 进入游戏大厅或游戏房间，这个session的生命周期是永久的
        // 退出游戏大厅或游戏房间，该session又被重新设置为临时的，在指定时间内无通信就被删除

        // 获取session
        session_ptr ssp = get_session_by_ssid(ssid);
        if (ssp == nullptr)
        {
            DLOG("get session by session id: %lu failed", ssid);

            return;
        }

        wsserver_t::timer_ptr tp = ssp->get_timer();
        // 1.在session的生命周期为永久的情况下，将生命周期设置为永久
        if (tp.get() == nullptr && ms == SESSION_PERMANENT)
        {
            // 无需处理
            return;
        }
        // 2.在session的生命周期为永久的情况下，设置指定时间后被移除的定时任务
        else if (tp.get() == nullptr && ms != SESSION_PERMANENT)
        {
            wsserver_t::timer_ptr tmp_tp = _server->set_timer(ms, std::bind(&session_manager::remove_session, this, ssid));
            ssp->set_timer(tmp_tp); // 将新的tp设置进session中
        }
        // 3.在session设置了定时移除任务的情况下，将生命周期设置为永久
        else if (tp.get() != nullptr && ms == SESSION_PERMANENT)
        {
            // 删除定时任务
            // 1.使用cancel()提前结束当前的定时任务
            tp->cancel(); // ！！cancel()并不是立即执行的！！
            ssp->set_timer(wsserver_t::timer_ptr());

            // 2.重新给_ssid_session中添加该session的信息
            _server->set_timer(0, std::bind(&session_manager::append_session, this, ssp)); // 这里添加已有session使用定时器来添加，原因是直接insert可能会出现先insert后cancel的情况，导致出错
        }
        // 4.在session设置了定时移除任务的情况下，重置session的生命周期
        else if (tp.get() != nullptr && ms != SESSION_PERMANENT)
        {
            // 1.使用cancel()提前结束当前的定时任务
            tp->cancel(); // 将session从_ssid_session中移除
            ssp->set_timer(wsserver_t::timer_ptr());

            // 2.将该session重新添加到_ssid_session中
            _server->set_timer(0, std::bind(&session_manager::append_session, this, ssp));
            
            // 3.给session设置新的定时任务
            wsserver_t::timer_ptr tmp_tp = _server->set_timer(ms, std::bind(&session_manager::remove_session, this, ssid));

            // 4.更新当前session关联的定时器
            ssp->set_timer(tmp_tp);
        }
    }

    // 移除session
    void remove_session(uint64_t ssid)
    {
        std::unique_lock<std::mutex> lock(_mtx);

        _ssid_session.erase(ssid);
    }

    ~session_manager()
    {
        DLOG("session管理模块即将销毁");
    }

private:
    uint64_t _next_ssid;                                     // session id 分配器
    std::mutex _mtx;                                         // 互斥锁
    std::unordered_map<uint64_t, session_ptr> _ssid_session; // session id和session的关联关系
    wsserver_t* _server; 
};