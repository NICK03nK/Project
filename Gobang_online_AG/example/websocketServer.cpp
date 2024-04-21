#include <iostream>
#include <string>
#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>

typedef websocketpp::server<websocketpp::config::asio> websocketsvr_t;

void wsopen_callback(websocketpp::connection_hdl hdl);
void wsclose_callback(websocketpp::connection_hdl hdl);
void wsmsg_callback(websocketpp::connection_hdl hdl, websocketsvr_t::message_ptr msg);
void http_callback(websocketpp::connection_hdl hdl);

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
    wssvr.set_open_handler(wsopen_callback);
    wssvr.set_close_handler(wsclose_callback);
    wssvr.set_message_handler(wsmsg_callback);
    wssvr.set_http_handler(http_callback);
    // 5.
    wssvr.listen(8080);
    // 6.
    wssvr.start_accept();
    // 7.
    wssvr.run();

    return 0;
}