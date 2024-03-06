// 给服务器发送一个数据，告诉服务器要发送1024字节的数据，但实际发送的数据不足1024字节，查看服务器处理结果

// 1.如果数据只发送一次，服务器将得不到完整的请求就不会进行业务处理，客户端也就得不到响应，最终超时关闭连接
// 2.连着给服务器发送了多次小的请求，服务器会将后边的请求当作是前面请求的正文呢来处理，而后面处理请求时可能会业务错误的处理而关闭连接

#include "../server.hpp"

int main()
{
    Socket cli_sock;
    cli_sock.CreateClient(8080, "127.0.0.1");

    std::string req = "GET /hello HTTP/1.1\r\nConnection: keep-alive\r\nContent-Length: 100\r\n\r\nbitejiuyeke";

    while (true)
    {
        assert(cli_sock.Send(req.c_str(), req.size()) != -1);
        assert(cli_sock.Send(req.c_str(), req.size()) != -1);
        assert(cli_sock.Send(req.c_str(), req.size()) != -1);
        assert(cli_sock.Send(req.c_str(), req.size()) != -1);
        assert(cli_sock.Send(req.c_str(), req.size()) != -1);

        char buf[1024] = { 0 };
        assert(cli_sock.Recv(buf, 1023));
        DBG_LOG("[%s]", buf);
        
        sleep(3);
    }
    cli_sock.Close();

    return 0;
}