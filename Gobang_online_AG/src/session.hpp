#pragma once

#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>
#include <mutex>
#include <unordered_map>
#include <memory>

#include "logger.hpp"

enum session_statu
{
    LOGIN,
    NOTLOGIN
};

typedef websocketpp::server<websocketpp::config::asio> websocketsvr_t;
class session
{
public:
    session(uint64_t session_id)
        :_session_id(session_id)
    {
        DLOG("session：%p 已创建", this);
    }

    ~session() { DLOG("session：%p 已销毁", this); }

    uint64_t get_session_id() { return _session_id; }

    void set_statu(session_statu statu) { _statu = statu; }

    void set_user(uint64_t user_id) { _user_id = user_id; }

    uint64_t get_user_id() { return _user_id; }

    void set_timer(const websocketsvr_t::timer_ptr& tp) { _tp = tp; }

    websocketsvr_t::timer_ptr get_timer() { return _tp; }

    bool is_login() { return _statu == LOGIN; }

private:
    uint64_t _session_id;          // session id
    session_statu _statu;          // 用户当前的状态信息（登录/未登录）
    uint64_t _user_id;             // 与session对应的用户id
    websocketsvr_t::timer_ptr _tp; // 与session关联的定时器
};

#define SESSION_TEMOPRARY 30000 // session的生命周期为30s
#define SESSION_PERMANENT -1    // session的生命周期为永久

typedef std::shared_ptr<session> session_ptr;
class session_manager
{
public:
    session_manager(websocketsvr_t* wssvr)
        :_next_session_id(1)
        , _wssvr(wssvr)
    {
        DLOG("session管理类初始化完毕");
    }

    ~session_manager() { DLOG("session管理类即将销毁"); }

    // 创建session
    session_ptr create_session(uint64_t user_id, session_statu statu)
    {
        std::unique_lock<std::mutex> lock(_mtx);

        session_ptr psession(new session(_next_session_id++));
        psession->set_user(user_id);
        psession->set_statu(statu);

        _session_id_and_session.insert({psession->get_session_id(), psession});

        return psession;
    }

    // 向_session_id_and_session中重新添加一个已存在的session
    void append_session(const session_ptr& psession)
    {
        std::unique_lock<std::mutex> lock(_mtx);

        _session_id_and_session.insert({psession->get_session_id(), psession});
    }

    // 通过sessin id获取session
    session_ptr get_session_by_session_id(uint64_t session_id)
    {
        std::unique_lock<std::mutex> lock(_mtx);

        auto it = _session_id_and_session.find(session_id);
        if (it == _session_id_and_session.end())
        {
            DLOG("不存在session id为：%d 的session", session_id);
            return session_ptr();
        }

        return it->second;
    }

    // 移除session
    void remove_session(uint64_t session_id)
    {
        std::unique_lock<std::mutex> lock(_mtx);

        _session_id_and_session.erase(session_id);
    }

    // 设置session过期时间
    void set_session_expiration_time(uint64_t session_id, int ms)
    {
        // 依赖于websocketpp的定时器来实现对session生命周期的管理

        // 用户登录后，创建session，此时的session是临时的，在指定时间内无通信就被删除
        // 进入游戏大厅或游戏房间，这个session的生命周期是永久的
        // 退出游戏大厅或游戏房间，该session又被重新设置为临时的，在指定时间内无通信就被删除

        // 获取session
        session_ptr psession = get_session_by_session_id(session_id);
        if (psession.get() == nullptr)
        {
            DLOG("通过session id获取session失败");
            return;
        }

        websocketsvr_t::timer_ptr tp = psession->get_timer();
        
        // 1. 在session的生命周期为永久的情况下，将session的生命周期设置为永久
        if (tp.get() == nullptr && ms == SESSION_PERMANENT)
        {
            // 无需处理
            return;
        }
        // 2. 在session的生命周期为永久的情况下，设置定时任务：超过指定时间移除该session
        else if (tp.get() == nullptr && ms != SESSION_PERMANENT)
        {
            websocketsvr_t::timer_ptr tmp_tp = _wssvr->set_timer(ms, std::bind(&session_manager::remove_session, this, session_id));
            psession->set_timer(tmp_tp);
        }
        // 3. 在session设置了定时移除任务的情况下，将session的生命周期设置为永久
        else if (tp.get() != nullptr && ms == SESSION_PERMANENT)
        {
            // 使用cancel()提前结束当前的定时任务，向session中添加一个空的定时器
            tp->cancel();
            psession->set_timer(websocketsvr_t::timer_ptr());

            // 重新向_session_id_and_session添加该session
            _wssvr->set_timer(0, std::bind(&session_manager::append_session, this, psession));
        }
        // 4. 在session设置了定时移除任务的情况下，重置session的生命周期
        else if (tp.get() != nullptr && ms != SESSION_PERMANENT)
        {
            // 使用cancel()提前结束当前的定时任务，向session中添加一个空的定时器
            tp->cancel();
            psession->set_timer(websocketsvr_t::timer_ptr());

            // 重新向_session_id_and_session添加该session
            _wssvr->set_timer(0, std::bind(&session_manager::append_session, this, psession));

            // 给session设置新的定时任务，并将定时器更新到session中
            websocketsvr_t::timer_ptr tmp_tp = _wssvr->set_timer(ms, std::bind(&session_manager::remove_session, this, session_id));
            psession->set_timer(tmp_tp);
        }
    }

private:
    uint64_t _next_session_id;                                         // session id分配器
    std::mutex _mtx;                                                   // 互斥锁
    std::unordered_map<uint64_t, session_ptr> _session_id_and_session; // session id和session的关联关系
    websocketsvr_t* _wssvr;                                            // Websocket服务器对象
};