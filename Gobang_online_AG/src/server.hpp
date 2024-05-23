#pragma once

#include <string>
#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>
#include <vector>

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
    // WebSocket长连接建立成功后的处理函数
    void wsopen_callback(websocketpp::connection_hdl hdl)
    {
        websocketsvr_t::connection_ptr conn = _wssvr.get_con_from_hdl(hdl);
        websocketpp::http::parser::request req = conn->get_request();
        std::string uri = req.get_uri();
        if (uri == "/hall") // 建立了游戏大厅的长连接
        {
            wsopen_game_hall(conn);
        }
        else if (uri == "/room") // 建立了游戏房间的长连接
        {
            wsopen_game_room(conn);
        }
    }

    // WebSocket长连接断开前的处理函数
    void wsclose_callback(websocketpp::connection_hdl hdl)
    {
        websocketsvr_t::connection_ptr conn = _wssvr.get_con_from_hdl(hdl);
        websocketpp::http::parser::request req = conn->get_request();
        std::string uri = req.get_uri();
        if (uri == "/hall") // 游戏大厅长连接断开
        {
            wscloes_game_hall(conn);
        }
        else if (uri == "/room") // 游戏房间长连接断开
        {
            wscloes_game_room(conn);
        }
    }

    // WebSocket长连接通信处理
    void wsmsg_callback(websocketpp::connection_hdl hdl, websocketsvr_t::message_ptr msg)
    {
        websocketsvr_t::connection_ptr conn = _wssvr.get_con_from_hdl(hdl);
        websocketpp::http::parser::request req = conn->get_request();
        std::string uri = req.get_uri();
        if (uri == "/hall") // 游戏大厅请求
        {
            wsmsg_game_hall(conn, msg); // 游戏大厅请求处理函数
        }
        else if (uri == "/room") // 游戏房间请求
        {
            wsmsg_game_room(conn, msg); // 游戏房间请求处理函数
        }
    }

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

    // 组织WebSocket响应
    void websocket_resp(websocketsvr_t::connection_ptr& conn, Json::Value& resp)
    {
        std::string resp_str;
        json_util::serialize(resp, resp_str);
        conn->send(resp_str);
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
            DLOG("反序列化注册信息失败");
            return http_resp(conn, false, "用户注册失败", websocketpp::http::status_code::value::bad_request);
        }

        // 3. 进行数据库的用户新增操作（成功返回200，失败返回400）
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

        // 用户注册成功，返回成功响应
        return http_resp(conn, true, "用户注册成功", websocketpp::http::status_code::value::ok);
    }

    // 用户登录请求的处理
    void login_handler(websocketsvr_t::connection_ptr& conn)
    {
        // 1. 获取HTTP请求正文
        std::string req_body = conn->get_request_body();

        // 2. 对HTTP请求正文进行反序列化得到用户名和密码
        Json::Value login_info;
        bool ret = json_util::unserialize(req_body, login_info);
        if (ret == false)
        {
            DLOG("反序列化登录信息失败");
            return http_resp(conn, false, "用户登录失败", websocketpp::http::status_code::value::bad_request);
        }

        // 3. 校验正文完整性，进行数据库的用户信息验证（失败返回400）
        if (login_info["username"].isNull() || login_info["password"].isNull())
        {
            DLOG("缺少用户名或密码");
            return http_resp(conn, false, "缺少用户名或密码", websocketpp::http::status_code::value::bad_request);
        }

        ret = _user_table.login(login_info); // 进行登录验证
        if (ret == false)
        {
            DLOG("用户登录失败");
            return http_resp(conn, false, "用户登录失败", websocketpp::http::status_code::value::bad_request);
        }

        // 4. 用户信息验证成功，则给客户端创建session
        uint64_t user_id = login_info["id"].asUInt64();
        session_ptr psession = _session_manager.create_session(user_id, LOGIN);
        if (psession.get() == nullptr)
        {
            DLOG("创建session失败");
            return http_resp(conn, false, "创建session失败", websocketpp::http::status_code::value::internal_server_error);
        }
        _session_manager.set_session_expiration_time(psession->get_session_id(), SESSION_TEMOPRARY);

        // 5. 设置响应头部，Set-Cookie，将session id通过cookie返回
        std::string cookie_ssid = "SSID=" + std::to_string(psession->get_session_id());
        conn->append_header("Set-Cookie", cookie_ssid);
        return http_resp(conn, true, "用户登录成功", websocketpp::http::status_code::value::ok);
    }

    // 通过HTTP请求头部字段中的Cookie信息获取session id
    bool get_cookie_value(const std::string& cookie, const std::string& key, std::string& value)
    {
        // Cookie: SSID=xxx; key=value; key=value; 
        // 1. 以‘; ’作为间隔，对字符串进行分割，得到各个单个的Cookie信息
        std::vector<std::string> cookie_arr;
        string_util::split(cookie, "; ", cookie_arr);

        // 2. 对单个的cookie字符串以‘=’为间隔进行分割，得到key和val
        for (const auto& str : cookie_arr)
        {
            std::vector<std::string> tmp_arr;
            string_util::split(str, "=", tmp_arr);
            if (tmp_arr.size() != 2) continue;
            
            if (tmp_arr[0] == key)
            {
                value = tmp_arr[1];
                return true;
            }
        }
        
        return false;
    }

    // 获取用户信息请求的处理
    void info_handler(websocketsvr_t::connection_ptr& conn)
    {
        // 1. 获取HTTP请求中的Cookie字段
        std::string cookie_str = conn->get_request_header("Cookie");
        if (cookie_str.empty())
        {
            return http_resp(conn, false, "没有Cookie信息", websocketpp::http::status_code::value::bad_request);
        }

        // 从Cookie中获取session id
        std::string session_id_str;
        bool ret = get_cookie_value(cookie_str, "SSID", session_id_str);
        if (ret == false)
        {
            return http_resp(conn, false, "Cookie中没有session id", websocketpp::http::status_code::value::bad_request);
        }

        // 2. 根据session id在session管理中获取对应的session
        session_ptr psession = _session_manager.get_session_by_session_id(std::stoul(session_id_str));
        if (psession.get() == nullptr)
        {
            return http_resp(conn, false, "session已过期", websocketpp::http::status_code::value::bad_request);
        }

        // 3. 通过session获取对应的user id，再从数据库中获取用户信息，并序列化返回给客户端
        uint64_t user_id = psession->get_user_id();
        Json::Value user_info;
        ret = _user_table.select_by_id(user_id, user_info);
        if (ret == false)
        {
            return http_resp(conn, false, "获取用户信息失败", websocketpp::http::status_code::value::bad_request);
        }

        std::string user_info_str;
        json_util::serialize(user_info, user_info_str);
        conn->set_status(websocketpp::http::status_code::value::ok);
        conn->set_body(user_info_str);
        conn->append_header("Content-Type", "application/json");

        // 4. 上述操作访问了session，所以要刷新session的过期时间
        _session_manager.set_session_expiration_time(psession->get_session_id(), SESSION_TEMOPRARY);
    }

    // 用于验证用户是否登录成功，登录成功则返回用户session
    session_ptr get_session_by_cookie(websocketsvr_t::connection_ptr conn)
    {
        Json::Value resp;

        // 1. 获取HTTP请求中的Cookie字段
        std::string cookie_str = conn->get_request_header("Cookie");
        if (cookie_str.empty())
        {
            resp["optype"] = "hall_ready";
            resp["result"] = false;
            resp["reason"] = "没有Cookie信息";
            websocket_resp(conn, resp);
            return session_ptr();
        }

        // 从Cookie中获取session id
        std::string session_id_str;
        bool ret = get_cookie_value(cookie_str, "SSID", session_id_str);
        if (ret == false)
        {
            resp["optype"] = "hall_ready";
            resp["result"] = false;
            resp["reason"] = "Cookie中没有session";
            websocket_resp(conn, resp);
            return session_ptr();
        }

        // 2. 根据session id在session管理中获取对应的session
        session_ptr psession = _session_manager.get_session_by_session_id(std::stoul(session_id_str));
        if (psession.get() == nullptr)
        {
            resp["optype"] = "hall_ready";
            resp["result"] = false;
            resp["reason"] = "session已过期";
            websocket_resp(conn, resp);
            return session_ptr();
        }

        return psession;
    }

    // 游戏大厅长连接建立成功的处理函数
    void wsopen_game_hall(websocketsvr_t::connection_ptr conn)
    {
        Json::Value resp;

        // 1. 登录验证（判断当前用户是否登录成功）
        session_ptr psession = get_session_by_cookie(conn);
        if (psession.get() == nullptr) return;

        // 2. 判断当前用户是否重复登录
        if (_user_online.is_in_game_hall(psession->get_user_id()) || _user_online.is_in_game_room(psession->get_user_id()))
        {
            resp["optype"] = "hall_ready";
            resp["result"] = false;
            resp["reason"] = "用户已登录";
            return websocket_resp(conn, resp);
        }

        // 3. 将当前用户和对应的连接加入到游戏大厅
        _user_online.enter_game_hall(psession->get_user_id(), conn);

        // 4. 给用户响应游戏大厅建立成功
        resp["optype"] = "hall_ready";
        resp["result"] = true;
        resp["uid"] = (Json::UInt64)psession->get_user_id();
        websocket_resp(conn, resp);

        // 5. 将session的生命周期设置为永久
        _session_manager.set_session_expiration_time(psession->get_session_id(), SESSION_PERMANENT);
    }

    // 游戏房间长连接建立成功的处理函数
    void wsopen_game_room(websocketsvr_t::connection_ptr conn)
    {
        Json::Value resp;

        // 1. 登录验证（判断当前用户是否登录成功）
        session_ptr psession = get_session_by_cookie(conn);
        if (psession.get() == nullptr) return;

        // 2. 判断当前用户是否重复登录
        if (_user_online.is_in_game_hall(psession->get_user_id()) || _user_online.is_in_game_room(psession->get_user_id()))
        {
            resp["optype"] = "room_ready";
            resp["result"] = false;
            resp["reason"] = "用户已登录";
            return websocket_resp(conn, resp);
        }

        // 3. 判断当前用户是否已经创建好了房间
        room_ptr proom = _room_manager.get_room_by_user_id(psession->get_user_id());
        if (proom.get() == nullptr)
        {
            resp["optype"] = "room_ready";
            resp["result"] = false;
            resp["reason"] = "通过用户id获取游戏房间失败";
            return websocket_resp(conn, resp);
        }

        // 4. 将当前用户添加到在线用户管理的游戏房间中的用户管理中
        _user_online.enter_game_room(psession->get_user_id(), conn);

        // 5. 给用户响应房间创建完成
        resp["optype"] = "room_ready";
        resp["result"] = true;
        resp["room_id"] = (Json::UInt64)proom->id();
        resp["uid"] = (Json::UInt64)psession->get_user_id();
        resp["white_id"] = (Json::UInt64)proom->get_white_player();
        resp["black_id"] = (Json::UInt64)proom->get_black_player();
        websocket_resp(conn, resp);

        // 6. 将session的生命周期设置为永久
        _session_manager.set_session_expiration_time(psession->get_session_id(), SESSION_PERMANENT);
    }

    // 游戏大厅长连接断开的处理函数
    void wscloes_game_hall(websocketsvr_t::connection_ptr conn)
    {
        // 1. 获取用户的session
        session_ptr psession = get_session_by_cookie(conn);
        if (psession.get() == nullptr) return;

        // 2. 将用户从游戏大厅中移除
        _user_online.exit_game_hall(psession->get_user_id());

        // 3. 将session的生命周期设为定时的，超时自动删除
        _session_manager.set_session_expiration_time(psession->get_session_id(), SESSION_TEMOPRARY);
    }

    // 游戏房间长连接断开的处理函数
    void wscloes_game_room(websocketsvr_t::connection_ptr conn)
    {
        // 1. 获取用户的session
        session_ptr psession = get_session_by_cookie(conn);
        if (psession.get() == nullptr) return;

        // 2. 将玩家从在线用户管理中的游戏房间中的玩家中移除
        _user_online.exit_game_room(psession->get_user_id());

        // 3. 将玩家从游戏房间中移除(房间中所有玩家都退出了就会销毁房间)
        _room_manager.remove_player_in_room(psession->get_user_id());

        // 4. 将session的生命周期设置为定时的，超时自动销毁
        _session_manager.set_session_expiration_time(psession->get_session_id(), SESSION_TEMOPRARY);
    }

    // 游戏大厅请求处理函数（游戏匹配请求/停止匹配请求）
    void wsmsg_game_hall(websocketsvr_t::connection_ptr conn, websocketsvr_t::message_ptr msg)
    {
        Json::Value resp;

        // 1. 获取用户的session
        session_ptr psession = get_session_by_cookie(conn);
        if (psession.get() == nullptr) return;

        // 2. 获取WebSocket请求信息
        std::string req_str = msg->get_payload();
        Json::Value req;
        bool ret = json_util::unserialize(req_str, req);
        if (ret == false)
        {
            resp["result"] = false;
            resp["reason"] = "解析请求失败";
            return websocket_resp(conn, resp);
        }

        // 3. 对于请求分别进行处理
        if (!req["optype"].isNull() && req["optype"].asString() == "match_start")
        {
            // 开始游戏匹配：通过匹配模块，将玩家添加到匹配队列中
            _match_manager.add(psession->get_user_id());
            resp["optype"] = "match_start";
            resp["result"] = true;
            return websocket_resp(conn, resp);
        }
        else if (!req["optype"].isNull() && req["optype"].asString() == "match_stop")
        {
            // 停止游戏匹配：通过匹配模块，将玩家从匹配队列中移除
            _match_manager.del(psession->get_user_id());
            resp["optype"] = "match_stop";
            resp["result"] = true;
            return websocket_resp(conn, resp);
        }

        resp["optype"] = "unknown";
        resp["result"] = false;
        resp["reason"] = "未知请求类型";

        return websocket_resp(conn, resp);
    }

    // 游戏房间请求处理函数（下棋请求/聊天请求）
    void wsmsg_game_room(websocketsvr_t::connection_ptr conn, websocketsvr_t::message_ptr msg)
    {
        Json::Value resp;

        // 1. 获取用户的session
        session_ptr psession = get_session_by_cookie(conn);
        if (psession.get() == nullptr) return;

        // 2. 获取用户所在的游戏房间信息
        room_ptr proom = _room_manager.get_room_by_user_id(psession->get_user_id());
        if (proom.get() == nullptr)
        {
            resp["optype"] = "unknown";
            resp["result"] = false;
            resp["reason"] = "通过用户id获取游戏房间失败";
            return websocket_resp(conn, resp);
        }

        // 3. 对请求进行反序列化
        std::string req_str = msg->get_payload();
        Json::Value req;
        bool ret = json_util::unserialize(req_str, req);
        if (ret == false)
        {
            resp["optype"] = "unknown";
            resp["result"] = false;
            resp["reason"] = "解析请求失败";
            return websocket_resp(conn, resp);
        }

        // 4. 通过游戏房间进行游戏房间请求的处理
        return proom->handle_request(req);
    }

private:
    std::string _web_root;            // 静态资源根目录（./wwwroot/） 请求的url为 /register.html，会自动将url拼接到_web_root后，即./wwwroot/register.html
    websocketsvr_t _wssvr;            // WebSocket服务器
    user_table _user_table;           // 用户数据管理模块
    online_manager _user_online;      // 在线用户管理模块
    room_manager _room_manager;       // 游戏房间管理模块
    match_manager _match_manager;     // 游戏匹配管理模块
    session_manager _session_manager; // session管理模块
};
