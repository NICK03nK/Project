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
    void wsopen_callback(websocketpp::connection_hdl hdl){}

    void wsclose_callback(websocketpp::connection_hdl hdl){}

    void wsmsg_callback(websocketpp::connection_hdl hdl, websocketsvr_t::message_ptr msg){}

    void http_callback(websocketpp::connection_hdl hdl)
    {
        websocketsvr_t::connection_ptr conn = _wssvr.get_con_from_hdl(hdl);
        websocketpp::http::parser::request req = conn->get_request();
        std::string method = req.get_method();
        std::string uri = req.get_uri();
        if (method == "POST" && uri == "/signup")
        {
            signup_handler(conn);
        }
        else if (method == "POST" && uri == "/login")
        {
            login_handler(conn);
        }
        else if (method == "GET" && uri == "/userinfo")
        {
            info_handler(conn);
        }
        else
        {
            file_handler(conn);
        }
    }

private:
    // 组织HTTP响应
    void http_resp(websocketsvr_t::connection_ptr& conn, bool result, const std::string& reason, websocketpp::http::status_code::value code)
    {
        Json::Value resp;

        resp["result"] = result;
        resp["reason"] = reason;

        std::string resp_str;
        json_util::serialize(resp, resp_str);

        conn->set_status(code);
        conn->set_body(resp_str);
        conn->append_header("Content-Type", "application/json");
    }

    // 静态资源请求的处理
    void file_handler(websocketsvr_t::connection_ptr& conn)
    {
        // 1.获取http请求的uri -- 资源路径，了解客户端请求的页面文件名称
        websocketpp::http::parser::request req = conn->get_request();
        std::string uri = req.get_uri();

        // 2.组合出文件实际的路径（相对根目录 + uri）
        std::string real_path = _web_root + uri;

        // 3.如果请求的uri是个目录，则增加一个后缀 -- login.html
        if (real_path.back() == '/') // 表示请求资源路径是一个目录
        {
            real_path += "login.html";
        }

        // 4.读取文件内容；若文件不存在，则返回404
        std::string body;
        bool ret = file_util::read(real_path, body);
        if (ret == false)
        {
            body += "<html>";
            body += "<head>";
            body += "<meta charset='UTF-8'/>";
            body += "</head>";
            body += "<body>";
            body += "<h1> Not Found </h1>";
            body += "</body>";

            conn->set_status(websocketpp::http::status_code::value::not_found);
            conn->set_body(body);

            return;
        }

        // 5.设置响应正文
        conn->set_status(websocketpp::http::status_code::value::ok);
        conn->set_body(body);

        return;
    }

    // 用户注册请求的处理
    void signup_handler(websocketsvr_t::connection_ptr& conn)
    {
        // 1. 获取HTTP请求正文
        std::string req_body = conn->get_request_body();

        // 2. 对HTTP请求正文进行反序列化得到用户名和密码
        Json::Value signup_info;
        bool ret = json_util::unserialize(req_body, signup_info);
        if (ret == false)
        {
            DLOG("序列化注册信息失败");
            return http_resp(conn, false, "用户注册失败", websocketpp::http::status_code::value::bad_request);
        }

        // 3. 进行数据库的用户新增操作(成功返回200，失败返回400)
        if (signup_info["username"].isNull() || signup_info["password"].isNull())
        {
            DLOG("缺少用户名或密码");
            return http_resp(conn, false, "缺少用户名或密码", websocketpp::http::status_code::value::bad_request);
        }

        ret = _user_table.signup(signup_info); // 在数据库中新增用户
        if (ret == false)
        {
            DLOG("向数据库中添加用户失败");
            return http_resp(conn, false, "用户注册失败", websocketpp::http::status_code::value::bad_request);
        }

        return http_resp(conn, true, "用户注册成功", websocketpp::http::status_code::value::ok);
    }

    // 用户登录请求的处理
    void login_handler(websocketsvr_t::connection_ptr& conn){}

    // 获取用户信息请求的处理
    void info_handler(websocketsvr_t::connection_ptr& conn){}

private:
    std::string _web_root;            // 静态资源根目录（./wwwroot/） 请求的url为 /register.html，会自动将url拼接到_web_root后，即./wwwroot/register.html
    websocketsvr_t _wssvr;            // WebSocket服务器
    user_table _user_table;           // 用户数据管理模块
    online_manager _user_online;      // 在线用户管理模块
    room_manager _room_manager;       // 游戏房间管理模块
    match_manager _match_manager;     // 游戏匹配管理模块
    session_manager _session_manager; // session管理模块
};