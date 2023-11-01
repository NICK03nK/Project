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
    // websocket长连接建立成功之后的处理函数
    void wsopen_callback(websocketpp::connection_hdl hdl)
    {
        wsserver_t::connection_ptr conn = _wssvr.get_con_from_hdl(hdl); // 获取连接

        websocketpp::http::parser::request req = conn->get_request(); // 获取http请求
        std::string uri = req.get_uri(); // 获取请求的uri

        if (uri == "/hall")
        {
            // 建立了游戏大厅的长连接
            return wsopen_game_hall(conn);
        }
        else if (uri == "/room")
        {
            // 建立了游戏房间的长连接
            return wsopen_game_room(conn);
        }
    }

    // websocket长连接断开前的处理函数
    void wsclose_callback(websocketpp::connection_hdl hdl)
    {
        wsserver_t::connection_ptr conn = _wssvr.get_con_from_hdl(hdl); // 获取连接

        websocketpp::http::parser::request req = conn->get_request(); // 获取http请求
        std::string uri = req.get_uri(); // 获取请求的uri

        if (uri == "/hall")
        {
            // 游戏大厅的长连接断开处理
            return wsclose_game_hall(conn);
        }
        else if (uri == "/room")
        {
            // 游戏房间的长连接断开处理
            return wsclose_game_room(conn);
        }
    }

    // websocket长连接通信处理
    void wsmsg_callback(websocketpp::connection_hdl hdl, wsserver_t::message_ptr msg)
    {
         wsserver_t::connection_ptr conn = _wssvr.get_con_from_hdl(hdl); // 获取连接

        websocketpp::http::parser::request req = conn->get_request(); // 获取http请求
        std::string uri = req.get_uri(); // 获取请求的uri

        if (uri == "/hall")
        {
            // 游戏大厅的长连接断开处理
            return wsmsg_game_hall(conn, msg);
        }
        else if (uri == "/room")
        {
            // 游戏房间的长连接断开处理
            return wsmsg_game_room(conn, msg);
        }
    }

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

    void websocket_resp(wsserver_t::connection_ptr& conn, Json::Value& resp)
    {
        std::string body;
        json_util::serialize(resp, body);

        conn->send(body);
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
        // 1.获取http请求正文
        std::string req_body = conn->get_request_body();

        // 2.对正文进行反序列化，得到用户名和密码
        Json::Value signup_info;
        bool ret = json_util::unserialize(req_body, signup_info);
        if (ret == false) // 反序列化失败
        {
            DLOG("unserialize sign up information failed");

            return http_resp(conn, false, "request body is incorrectly formatted", websocketpp::http::status_code::value::bad_request);
        }

        // 3.进行数据库的用户新增操作(成功返回200，失败返回400)
        if (signup_info["username"].isNull() || signup_info["password"].isNull())
        {
            DLOG("username or password is incomplete");

            return http_resp(conn, false, "username or password is empty", websocketpp::http::status_code::value::bad_request);
        }

        ret = _ut.signup(signup_info); // 将用户名和密码新增至数据库中
        if (ret == false)
        {
            DLOG("insert data into database failed");

            return http_resp(conn, false, "The username has been used", websocketpp::http::status_code::value::bad_request);
        }

        // 成功将用户名和密码加入数据库
        return http_resp(conn, true, "sign up successfully", websocketpp::http::status_code::value::ok);
    }

    // 用户登录功能请求的处理
    void signin(wsserver_t::connection_ptr& conn)
    {
        // 1.获取http请求正文
        std::string req_body = conn->get_request_body();

        // 2.对正文进行反序列化，得到用户名和密码
        Json::Value signin_info;
        bool ret = json_util::unserialize(req_body, signin_info);
        if (ret == false) // 反序列化失败
        {
            DLOG("unserialize sign in information failed");

            return http_resp(conn, false, "request body is incorrectly formatted", websocketpp::http::status_code::value::bad_request);
        }

        // 3.校验正文完整性，进行数据库的用户信息验证(失败返回400)
        if (signin_info["username"].isNull() || signin_info["password"].isNull())
        {
            DLOG("username or password is incomplete");

            return http_resp(conn, false, "username or password is empty", websocketpp::http::status_code::value::bad_request);
        }

        ret = _ut.signin(signin_info);
        if (ret == false)
        {
            DLOG("sign in failed");

            return http_resp(conn, false, "sign in failed", websocketpp::http::status_code::value::bad_request);
        }

        // 4.用户信息验证成功，则给客户端创建session
        uint64_t userId = signin_info["id"].asUInt64();
        session_ptr ssp = _sm.create_session(userId, LOGIN);
        if (ssp.get() == nullptr)
        {
            DLOG("create session failed");

            return http_resp(conn, false, "create session failed", websocketpp::http::status_code::value::internal_server_error);
        }
        _sm.set_session_expiration_time(ssp->ssid(), SESSION_TEMOPRARY); // 给session设置生命周期

        // 5.设置响应头部，Set-Cookie，将session id通过cookie返回
        std::string cookie_ssid = "SSID=" + std::to_string(ssp->ssid());
        conn->append_header("Set-Cookie", cookie_ssid);
        return http_resp(conn, true, "sign in successfully", websocketpp::http::status_code::value::ok);
    }

    bool get_cookie_value(const std::string& cookie_str, const std::string& key, std::string& val)
    {
        // Cookie: SSID=xxx; path=/;
        // 1.以‘; ’作为间隔，对字符串进行分割，得到各个单个的cookie信息
        std::vector<std::string> cookie_arr;
        string_util::split(cookie_str, "; ", cookie_arr);

        // 2.对单个的cookie字符串以‘=’为间隔进行分割，得到key和val
        for (auto str : cookie_arr)
        {
            std::vector<std::string> tmp_arr;
            string_util::split(str, "=", tmp_arr);
            if (tmp_arr.size() != 2)
            {
                continue;
            }

            if (tmp_arr[0] == key)
            {
                val = tmp_arr[1];
                return true;
            }

            return false;
        }
    }

    // 获取用户信息功能请求的处理
    void info(wsserver_t::connection_ptr& conn)
    {
        // 1.获取http请求中的Cookie字段
        std::string cookie_str = conn->get_request_header("Cookie");
        if (cookie_str.empty())
        {
            // 没有cookie，则返回错误：没有cookie，重新登陆
            return http_resp(conn, false, "cannot find cookie", websocketpp::http::status_code::value::bad_request);
        }

        // 从Cookie中获取session id
        std::string ssid_str;
        bool ret = get_cookie_value(cookie_str, "SSID", ssid_str);
        if (ret == false)
        {
            // cookie中没有ssid，则返回错误：没有ssid，重新登陆
            return http_resp(conn, false, "there is no session id in cookie", websocketpp::http::status_code::value::bad_request);
        }

        // 2.根据ssid在session管理中查找对应的session
        session_ptr ssp = _sm.get_session_by_ssid(std::stoul(ssid_str));
        if (ssp.get() == nullptr)
        {
            // 没找到session则表示登录已过期，重新登陆
            return http_resp(conn, false, "sign in expiration, cannot find session by session id", websocketpp::http::status_code::value::bad_request);
        }

        // 3.通过session获取对应的用户id，再从数据库中获取用户信息，并序列化数据返回给客户端
        uint64_t userId = ssp->get_user();
        Json::Value user_info;
        ret = _ut.select_by_id(userId, user_info);
        if (ret == false)
        {
            // 获取用户信息失败，返回错误：找不到用户信息
            return http_resp(conn, false, "get user's information failed", websocketpp::http::status_code::value::bad_request);
        }

        std::string body;
        json_util::serialize(user_info, body);
        conn->set_body(body);
        conn->append_header("Content-Type", "application/json");
        conn->set_status(websocketpp::http::status_code::value::ok);

        // 4.由于访问过session，所以要刷新session的生命周期
        _sm.set_session_expiration_time(ssp->ssid(), SESSION_TEMOPRARY);
    }

    session_ptr get_session_by_cookie(wsserver_t::connection_ptr conn)
    {
        // 1.登录验证(判断当前客户端是否登录成功)
        Json::Value err_resp;

        // 1.1.获取http请求中的Cookie字段
        std::string cookie_str = conn->get_request_header("Cookie");
        if (cookie_str.empty())
        {
            // 没有cookie，则返回错误：没有cookie，重新登陆
            err_resp["optype"] = "hall_ready";
            err_resp["result"] = false;
            err_resp["reason"] = "cannot find cookie";

            websocket_resp(conn, err_resp);

            return session_ptr();
        }

        // 从Cookie中获取session id
        std::string ssid_str;
        bool ret = get_cookie_value(cookie_str, "SSID", ssid_str);
        if (ret == false)
        {
            // cookie中没有ssid，则返回错误：没有ssid，重新登陆
            err_resp["optype"] = "hall_ready";
            err_resp["result"] = false;
            err_resp["reason"] = "there is no session id in cookie";

            websocket_resp(conn, err_resp);

            return session_ptr();
        }

        // 1.2.根据ssid在session管理中查找对应的session
        session_ptr ssp = _sm.get_session_by_ssid(std::stoul(ssid_str));
        if (ssp.get() == nullptr)
        {
            // 没找到session则表示登录已过期，重新登陆
            err_resp["optype"] = "hall_ready";
            err_resp["result"] = false;
            err_resp["reason"] = "sign in expiration, cannot find session by session id";

            websocket_resp(conn, err_resp);

            return session_ptr();
        }

        return ssp;
    }

    // 游戏大厅长连接建立成功后处理函数
    void wsopen_game_hall(wsserver_t::connection_ptr conn)
    {
        Json::Value resp_json;

        // 1.登录验证(判断当前客户端是否登录成功)
        session_ptr ssp = get_session_by_cookie(conn);
        if (ssp.get() == nullptr)
        {
            return;
        }

        // 2.判断当前客户端是否重复登录
        if (_om.is_in_game_hall(ssp->get_user()) || _om.is_in_game_room(ssp->get_user()))
        {
            resp_json["optype"] = "hall_ready";
            resp_json["result"] = false;
            resp_json["reason"] = "repeated player logins";

            return websocket_resp(conn, resp_json);
        }

        // 3.将当前客户端以及连接加入到游戏大厅
        _om.enter_game_hall(ssp->get_user(), conn);

        // 4.给客户端响应游戏大厅建立成功
        resp_json["optype"] = "hall_ready";
        resp_json["result"] = true;
        resp_json["userId"] = (Json::UInt64)ssp->get_user();
        
        websocket_resp(conn, resp_json);

        // 5.将session生命周期设置为永久
        _sm.set_session_expiration_time(ssp->ssid(), SESSION_PERMANENT);
    }

    // 游戏房间长连接建立成功后处理函数
    void wsopen_game_room(wsserver_t::connection_ptr conn)
    {
        Json::Value resp_json;

        // 1.获取当前客户端的session
        session_ptr ssp = get_session_by_cookie(conn);
        if (ssp.get() == nullptr)
        {
            return;
        }

        // 2.判断当前客户端是否重复登录
        if (_om.is_in_game_hall(ssp->get_user()) || _om.is_in_game_room(ssp->get_user()))
        {
            resp_json["optype"] = "room_ready";
            resp_json["result"] = false;
            resp_json["reason"] = "repeated player logins";

            return websocket_resp(conn, resp_json);
        }

        // 3.判断当前用户是否已经创建好了房间
        room_ptr rp = _rm.get_room_by_userId(ssp->get_user());
        if (rp.get() == nullptr)
        {
            resp_json["optype"] = "room_ready";
            resp_json["result"] = false;
            resp_json["reason"] = "get game room information by player's id failed";

            return websocket_resp(conn, resp_json);
        }

        // 4.将当前用户添加到在线用户管理的游戏房间中
        _om.enter_game_room(ssp->get_user(), conn);

        // 5.将session生命周期设置为永久
        _sm.set_session_expiration_time(ssp->ssid(), SESSION_PERMANENT);

        // 6.给客户端响应房间创建完成
        resp_json["optype"] = "room_ready";
        resp_json["result"] = true;
        resp_json["room_id"] = (Json::UInt64)rp->id();
        resp_json["uid"] = (Json::UInt64)ssp->get_user();
        resp_json["white_id"] = (Json::UInt64)rp->get_white_player();
        resp_json["black_id"] = (Json::UInt64)rp->get_black_player();

        return websocket_resp(conn, resp_json);
    }

    // 游戏大厅长连断开后处理函数
    void wsclose_game_hall(wsserver_t::connection_ptr conn)
    {
        // 1.登录验证(判断当前客户端是否登录成功)
        session_ptr ssp = get_session_by_cookie(conn);
        if (ssp.get() == nullptr)
        {
            return;
        }

        // 2.将玩家从游戏大厅移除
        _om.exit_game_hall(ssp->get_user());

        // 3.将session的生命周期设置为定时的，超时自动销毁
        _sm.set_session_expiration_time(ssp->ssid(), SESSION_TEMOPRARY);
    }

    // 游戏房间长连断开后处理函数
    void wsclose_game_room(wsserver_t::connection_ptr conn)
    {
        // 1.获取会话信息，识别客户端
        session_ptr ssp = get_session_by_cookie(conn);
        if (ssp.get() == nullptr)
        {
            return;
        }

        // 2.将玩家从在线用户管理中移除
        _om.exit_game_room(ssp->get_user());

        // 3.将session的生命周期设置为定时的，超时自动销毁
        _sm.set_session_expiration_time(ssp->ssid(), SESSION_TEMOPRARY);
        
        // 4.将玩家从游戏房间中移除(房间中所有玩家都退出了就会销毁房间)
        _rm.remove_room_player(ssp->get_user());
    }

    // 游戏大厅消息请求处理函数
    void wsmsg_game_hall(wsserver_t::connection_ptr conn, wsserver_t::message_ptr msg)
    {
        Json::Value resp_json;
        std::string resp_body;

        // 1.登录验证(判断当前客户端是否登录成功)
        session_ptr ssp = get_session_by_cookie(conn);
        if (ssp.get() == nullptr)
        {
            return;
        }

        // 2.获取请求信息
        std::string req_body = msg->get_payload();
        Json::Value req_json;
        bool ret = json_util::unserialize(req_body, req_json);
        if (ret == false)
        {
            resp_json["result"] = false;
            resp_json["reason"] = "resolution request failed";

            return websocket_resp(conn, resp_json);
        }

        // 3.对于请求进行处理
        if (!req_json["optype"].isNull() && req_json["optype"].asString() == "match_start")
        {
            // 开始对战匹配：通过匹配模块，将玩家添加到匹配队列中
            _mm.add(ssp->get_user());
            resp_json["optype"] = "match_start";
            resp_json["result"] = true;

            return websocket_resp(conn, resp_json);
        }
        else if (!req_json["optype"].isNull() && req_json["optype"].asString() == "match_stop")
        {
            // 停止对战匹配：通过匹配模块，将玩家从匹配队列中移除
            _mm.del(ssp->get_user());
            resp_json["optype"] = "match_stop";
            resp_json["result"] = true;

            return websocket_resp(conn, resp_json);
        }
        
        resp_json["optype"] = "unknown";
        resp_json["reason"] = "unknown response type";
        resp_json["result"] = false;

        return websocket_resp(conn, resp_json);
    }

    // 游戏房间消息请求处理函数
    void wsmsg_game_room(wsserver_t::connection_ptr conn, wsserver_t::message_ptr msg)
    {
        Json::Value resp_json;

        // 1.获取客户端session，识别客户端身份
        session_ptr ssp = get_session_by_cookie(conn);
        if (ssp.get() == nullptr)
        {
            return;
        }

        // 2.获取客户端房间信息
        room_ptr rp = _rm.get_room_by_userId(ssp->get_user());
        if (rp.get() == nullptr)
        {
            resp_json["optype"] = "unknown";
            resp_json["result"] = false;
            resp_json["reason"] = "get game room information by player's id failed";

            return websocket_resp(conn, resp_json);
        }

        // 3.对消息进行反序列化
        Json::Value req_json;
        std::string req_body = msg->get_payload();
        bool ret = json_util::unserialize(req_body, req_json);
        if (ret = false)
        {
            resp_json["result"] = false;
            resp_json["reason"] = "resolution request failed";

            return websocket_resp(conn, resp_json);
        }

        // 4.通过房间模块进行消息请求的处理
        return rp->handle_request(req_json);
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