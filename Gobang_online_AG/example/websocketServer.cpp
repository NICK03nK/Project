#include <iostream>
#include <string>
#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>

typedef websocketpp::server<websocketpp::config::asio> websocketsvr_t;

void wsopen_callback(websocketsvr_t* wssvr, websocketpp::connection_hdl hdl)
{
    std::cout << "websocket建立连接" << std::endl;
}

void wsclose_callback(websocketsvr_t* wssvr, websocketpp::connection_hdl hdl)
{
    std::cout << "websocket连接断开" << std::endl;
}

void wsmsg_callback(websocketsvr_t* wssvr, websocketpp::connection_hdl hdl, websocketsvr_t::message_ptr msg)
{
    // 获取通信连接
    websocketsvr_t::connection_ptr conn = wssvr->get_con_from_hdl(hdl);

    std::cout << "msg: " << msg->get_payload() << std::endl;
    
    // 将客户端发送的信息作为响应
    std::string resp = "client say: " + msg->get_payload();

    // 将响应信息发送给客户端
    conn->send(resp);
}

// 给客户端返回一个hello world页面
void http_callback(websocketsvr_t* wssvr, websocketpp::connection_hdl hdl)
{
    // 获取通信连接
    websocketsvr_t::connection_ptr conn = wssvr->get_con_from_hdl(hdl);

    // 打印请求正文
    std::cout << "body: " << conn->get_request_body() << std::endl;

    // 获取http请求
    websocketpp::http::parser::request req = conn->get_request();

    // 打印请求方法和url
    std::cout << "method: " << req.get_method() << std::endl;
    std::cout << "uri: " << req.get_uri() << std::endl;

    // 设置响应正文
    std::string body = "<html><body><h1>Hello World</h1></body></html>";
    conn->set_body(body);
    conn->append_header("Content-Type", "text/html");
    conn->set_status(websocketpp::http::status_code::ok);
}

int main()
{
    // 1. 实例化server对象
    // 2. 设置日志等级
    // 3. 初始化asio调度器，设置地址重用
    // 4. 设置回调函数
    // 5. 设置监听端口
    // 6. 开始获取新连接
    // 7. 启动服务器

    // 1.
    websocketsvr_t wssvr;
    // 2.
    wssvr.set_access_channels(websocketpp::log::alevel::none); // 禁止打印所有日志
    // 3.
    wssvr.init_asio();
    wssvr.set_reuse_addr(true);
    // 4.
    wssvr.set_open_handler(std::bind(wsopen_callback, &wssvr, std::placeholders::_1));
    wssvr.set_close_handler(std::bind(wsclose_callback, &wssvr, std::placeholders::_1));
    wssvr.set_message_handler(std::bind(wsmsg_callback, &wssvr, std::placeholders::_1, std::placeholders::_2));
    wssvr.set_http_handler(std::bind(http_callback, &wssvr, std::placeholders::_1));
    // 5.
    wssvr.listen(8080);
    // 6.
    wssvr.start_accept();
    // 7.
    wssvr.run();

    return 0;
}