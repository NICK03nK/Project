#pragma once

#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>

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

    bool is_login() { return _statu == LOGIN; }

private:
    uint64_t _session_id;          // session id
    session_statu _statu;          // 用户当前的状态信息（登录/未登录）
    uint64_t _user_id;             // 与session对应的用户id
    websocketsvr_t::timer_ptr _tp; // 与session关联的定时器
};