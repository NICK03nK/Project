#pragma once

#include <string>
#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>

#include "db.hpp"
#include "online.hpp"
#include "room.hpp"
#include "matcher.hpp"
#include "session.hpp"

#define WWWROOT "./wwwroot/"

class gobang_server
{
public:
    // 进行成员变量初始化，以及设置服务器回调函数
    gobang_server(const std::string& host,
                  const std::string& user,
                  const std::string& password,
                  const std::string& dbname,
                  uint32_t port = 3306,
                  const std::string& wwwroot = WWWROOT)
        :_web_root(wwwroot)
        , _user_table(host, user, password, dbname, port)
        , _room_manager(&_user_table, &_user_online)
        , _match_manager(&_room_manager, &_user_table, &_user_online)
        , _session_manager(&_wssvr)
    {
        _wssvr.set_access_channels(websocketpp::log::alevel::none);
        _wssvr.init_asio();
        _wssvr.set_reuse_addr(true);

        _wssvr.set_open_handler(std::bind(&gobang_server::wsopen_callback, this, std::placeholders::_1));
        _wssvr.set_close_handler(std::bind(&gobang_server::wsclose_callback, this, std::placeholders::_1));
        _wssvr.set_message_handler(std::bind(&gobang_server::wsmsg_callback, this, std::placeholders::_1, std::placeholders::_2));
        _wssvr.set_http_handler(std::bind(&gobang_server::http_callback, this, std::placeholders::_1));
    }

    // 启动服务器
    void start(uint16_t port)
    {
        _wssvr.listen(port);
        _wssvr.start_accept();
        _wssvr.run();
    }

private:
    void wsopen_callback(websocketpp::connection_hdl hdl);

    void wsclose_callback(websocketpp::connection_hdl hdl);

    void wsmsg_callback(websocketpp::connection_hdl hdl, websocketsvr_t::message_ptr msg);

    void http_callback(websocketpp::connection_hdl hdl);

private:
    std::string _web_root;            // 静态资源根目录（./wwwroot/） 请求的url为 /register.html，会自动将url拼接到_web_root后，即./wwwroot/register.html
    websocketsvr_t _wssvr;            // WebSocket服务器
    user_table _user_table;           // 用户数据管理模块
    online_manager _user_online;      // 在线用户管理模块
    room_manager _room_manager;       // 游戏房间管理模块
    match_manager _match_manager;     // 游戏匹配管理模块
    session_manager _session_manager; // session管理模块
};