#include <iostream>
#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>
#include <string>

using wsserver_t = websocketpp::server<websocketpp::config::asio>;

void wsopen_callback(wsserver_t* svr, websocketpp::connection_hdl hdl)
{
    std::cout << "websocket握手成功！" << std::endl;
}

void wsclose_callback(wsserver_t* svr, websocketpp::connection_hdl hdl)
{
    std::cout << "websocket连接断开！" << std::endl;
}

void wsmsg_callback(wsserver_t* svr, websocketpp::connection_hdl hdl, wsserver_t::message_ptr msg)
{
    wsserver_t::connection_ptr con = svr->get_con_from_hdl(hdl);
    
    std::cout << "wsmsg: " << msg->get_payload() << std:: endl;
    std::string rsp = "client say " + msg->get_payload();
    con->send(rsp, websocketpp::frame::opcode::value::text);  // 将响应发回给客户端
}

void http_callback(wsserver_t* svr, websocketpp::connection_hdl hdl)
{
    // 给客户端返回一个hello world页面

    wsserver_t::connection_ptr con = svr->get_con_from_hdl(hdl);  // 从hdl获取一个连接
    std::cout << "body: " << con->get_request_body() << std::endl;  // 打印请求正文

    websocketpp::http::parser::request req = con->get_request();  // 获取请求
    std::cout << "method: " << req.get_method() << std::endl;  // 打印请求的方法
    std::cout << "uri: " << req.get_uri() << std::endl;  // 打印请求的uri
    
    std::string body = "<html><body><h1>Hello World</h1></body></html>";
    con->set_body(body);  // 设置响应正文
    con->append_header("Content-Type", "text/html");  // 设置响应头部字段
    con->set_status(websocketpp::http::status_code::value::ok);  // 设置响应状态码
}

int main()
{
    // 1.实例化websocket的server对象
    wsserver_t wssvr;

    // 2.设置日志等级
    wssvr.set_access_channels(websocketpp::log::alevel::none);  // 禁止打印所有日志

    // 3.初始化asio调度器，设置地址重用
    wssvr.init_asio();
    wssvr.set_reuse_addr(true);

    // 4.设置回调函数
    wssvr.set_open_handler(std::bind(wsopen_callback, &wssvr, std::placeholders::_1));
    wssvr.set_close_handler(std::bind(wsclose_callback, &wssvr, std::placeholders::_1));
    wssvr.set_message_handler(std::bind(wsmsg_callback, &wssvr, std::placeholders::_1, std::placeholders::_2));
    wssvr.set_http_handler(std::bind(http_callback, &wssvr, std::placeholders::_1));

    // 5.设置监听端口
    wssvr.listen(8080);

    // 6.获取新连接
    wssvr.start_accept();

    // 7.启动服务器
    wssvr.run();

    return 0;
}