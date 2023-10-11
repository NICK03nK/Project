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

        if (method == "POST" && uri == "/reg")
        {
            return signup(conn);
        }
        else if (method == "POST" && uri == "/login")
        {
            return signin(conn);
        }
        else if (method == "GET" && uri == "/userinfo")
        {
            return info(conn);
        }
        else
        {
            return file_handler(conn);
        }
    }

private:
    void http_resp(wsserver_t::connection_ptr& conn, bool result, const std::string& reason, websocketpp::http::status_code::value code)
    {
        Json::Value resp_json;

        resp_json["result"] = result;
        resp_json["reason"] = reason;

        std::string resp_body;
        json_util::serialize(resp_json, resp_body);

        conn->set_status(code);
        conn->set_body(resp_body);
        conn->append_header("Content-Type", "application/json");
    }

    // 静态资源请求的处理
    void file_handler(wsserver_t::connection_ptr& conn)
    {
        // 1.获取http请求的uri -- 资源路径，了解客户端请求的页面文件名称
        websocketpp::http::parser::request req = conn->get_request(); // 获取http请求
        std::string uri = req.get_uri(); // 获取请求的uri

        // 2.组合出文件实际的路径(相对根目录 + uri)
        std::string realpath = _web_root + uri;

        // 3.如果请求的uri是个目录，则增加一个后缀 -- login.html
        if (realpath.back() == '/')
        {
            realpath += "login.html";
        }

        // 4.读取文件内容；若文件不存在，则返回404
        std::string body;
        bool ret = file_util::read(realpath, body);
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
    }

    // 用户注册功能请求的处理
    void signup(wsserver_t::connection_ptr& conn)
    {
        websocketpp::http::parser::request req = conn->get_request();

        // 1.获取http请求正文
        std::string req_body = conn->get_request_body();

        // 2.对正文进行反序列化，得到用户名和密码
        Json::Value signup_info;
        Json::Value resp_json;
        bool ret = json_util::unserialize(req_body, signup_info);
        if (ret == false) // 反序列化失败
        {
            DLOG("unserialize failed");

            return http_resp(conn, false, "request body is incorrectly formatted", websocketpp::http::status_code::value::bad_request);
        }

        // 3.进行数据库的用户新增操作(成功返回200，失败返回400)
        if (signup_info["username"].isNull() || signup_info["password"].isNull())
        {
            DLOG("username or password is incomplete");

            return http_resp(conn, false, "username or password is empty", websocketpp::http::status_code::value::bad_request);;
        }

        ret = _ut.signup(signup_info); // 将用户名和密码新增至数据库中
        if (ret == false)
        {
            DLOG("insert data into database failed");

            return http_resp(conn, false, "The username has been used", websocketpp::http::status_code::value::bad_request);
        }

        // 成功将用户名和密码加入数据库
        return http_resp(conn, true, "sign up successfull", websocketpp::http::status_code::value::ok);
    }

    // 用户登录功能请求的处理
    void signin(wsserver_t::connection_ptr& conn)
    {}

    // 获取用户信息功能请求的处理
    void info(wsserver_t::connection_ptr& conn)
    {}

private:
    std::string _web_root; // 静态资源根目录  ./wwwroot/  /register.html --> ./wwwroot/register.html
    wsserver_t _wssvr;     // 服务器模块
    user_table _ut;        // 用户数据管理模块
    online_manager _om;    // 在线用户管理模块
    room_manager _rm;      // 游戏房间管理模块
    match_manager _mm;     // 匹配管理模块
    session_manager _sm;   // session管理模块
};