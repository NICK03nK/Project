#include "../server.hpp"

int main()
{
    Socket cli_sock;
    cli_sock.CreateClient(8080, "127.0.0.1");

    std::string str = "hello";
    cli_sock.Send(str.c_str(), str.size());

    char buf[1024] = { 0 };
    cli_sock.Recv(buf, 1023);
    DBG_LOG("%s", buf);

    return 0;
}