#pragma once

#include "db.hpp"
#include "matcher.hpp"
#include "online.hpp"
#include "room.hpp"
#include "session.hpp"

#define WWWROOT "./wwwroot/"

class gobang_server
{
public:
    // 进行成员初始化，以及服务器回调函数的设置
    gobang_server(const std::string &host,
                  const std::string &user,
                  const std::string &password,
                  const std::string &dbname,
                  unsigned int port = 3306,
                  const std::string& wwwroot = WWWROOT)
        :_web_root(wwwroot)
        , _ut(host, user, password, dbname, port)
        , _rm(&_ut, &_om)
        , _mm(&_rm, &_ut, &_om)
        , _sm(&_wssvr)
    {
        // 设置日志等级
        _wssvr.set_access_channels(websocketpp::log::alevel::none); // 禁止打印所有日志

        // 初始化asio调度器，设置地址重用
        _wssvr.init_asio();
        _wssvr.set_reuse_addr(true);

        // 设置回调函数
        _wssvr.set_open_handler(std::bind(&gobang_server::wsopen_callback, this, std::placeholders::_1));
        _wssvr.set_close_handler(std::bind(&gobang_server::wsclose_callback, this, std::placeholders::_1));
        _wssvr.set_message_handler(std::bind(&gobang_server::wsmsg_callback, this, std::placeholders::_1, std::placeholders::_2));
        _wssvr.set_http_handler(std::bind(&gobang_server::http_callback, this, std::placeholders::_1));
    }

    // 启动服务器
    void start(int port)
    {
        // 设置监听端口
        _wssvr.listen(port);

        // 获取新连接
        _wssvr.start_accept();

        // 启动服务器
        _wssvr.run();
    }

private:
    void wsopen_callback(websocketpp::connection_hdl hdl)
    {}

    void wsclose_callback(websocketpp::connection_hdl hdl)
    {}

    void wsmsg_callback(websocketpp::connection_hdl hdl, wsserver_t::message_ptr msg)
    {}

    void http_callback(websocketpp::connection_hdl hdl)
    {
        wsserver_t::connection_ptr conn = _wssvr.get_con_from_hdl(hdl); // 获取连接

        websocketpp::http::parser::request req = conn->get_request(); // 获取http请求
        std::string method = req.get_method(); // 获取请求的方法
        std::string uri = req.get_uri(); // 获取请求的uri

        std::string pathname = _web_root + uri;
        std::string body;

        // 将请求的静态资源数据读取到body中
        file_util::read(pathname, body);

        conn->set_status(websocketpp::http::status_code::ok); // 设置响应状态码
        conn->set_body(body); // 设置http响应正文
    }

private:
    std::string _web_root; // 静态资源根目录  ./wwwroot/  /register.html --> ./wwwroot/register.html
    wsserver_t _wssvr;     // 服务器模块
    user_table _ut;        // 用户数据管理模块
    online_manager _om;    // 在线用户管理模块
    room_manager _rm;      // 游戏房间管理模块
    match_manager _mm;     // 匹配管理模块
    session_manager _sm;   // session管理模块
};