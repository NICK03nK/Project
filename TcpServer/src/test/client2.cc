// 超时连接测试1：创建一个客户端，给服务器发送一次数据后就停止操作，查看服务器是否会正常的超时关闭连接
#include "../server.hpp"

int main()
{
    Socket cli_sock;
    cli_sock.CreateClient(8080, "127.0.0.1");

    std::string req = "GET /hello HTTP/1.1\r\nConnection: keep-alive\r\nContent-Length: 0\r\n\r\n";

    while (true)
    {
        assert(cli_sock.Send(req.c_str(), req.size()) != -1);

        char buf[1024] = { 0 };
        assert(cli_sock.Recv(buf, 1023) != -1);
        DBG_LOG("[%s]", buf);
        
        sleep(15);
    }
    cli_sock.Close();

    return 0;
}