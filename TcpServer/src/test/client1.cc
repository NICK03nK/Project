// 长连接测试1：创建一个客户端持续给服务器发送数据，直到超过超时时间，看看是否正常运行
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
        
        sleep(3);
    }
    cli_sock.Close();

    return 0;
}