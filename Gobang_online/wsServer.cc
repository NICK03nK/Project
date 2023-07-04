#include <iostream>
#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>

using wsserver_t = websocketpp::server<websocketpp::config::asio>;

void http_callback(wsserver_t* svr, websocketpp::connection_hdl hdl)
{}

void wsopen_callback(wsserver_t* svr, websocketpp::connection_hdl hdl)
{}

void wsclose_callback(wsserver_t* svr, websocketpp::connection_hdl hdl)
{}

void wsmsg_callback(wsserver_t* svr, websocketpp::connection_hdl hdl, wsserver_t::message_ptr)
{}

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
    wssvr.set_http_handler(std::bind(http_callback, &wssvr, std::placeholders::_1));
    wssvr.set_open_handler(std::bind(wsopen_callback, &wssvr, std::placeholders::_1));
    wssvr.set_close_handler(std::bind(wsclose_callback, &wssvr, std::placeholders::_1));
    wssvr.set_message_handler(std::bind(wsmsg_callback, &wssvr, std::placeholders::_1, std::placeholders::_2));

    // 5.设置监听端口
    wssvr.listen(8080);

    // 6.获取新连接
    wssvr.start_accept();

    // 7.启动服务器
    wssvr.run();

    return 0;
}