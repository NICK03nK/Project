#include "../server.hpp"

void OnConnected(const SharedConnection& conn)
{
    DBG_LOG("new connection: %p", conn.get());
}

void OnClosed(const SharedConnection& conn)
{
    DBG_LOG("close connection: %p", conn.get());
}

void OnMessage(const SharedConnection& conn, Buffer* buf)
{
    DBG_LOG("%s", buf->GetReadPos());
    buf->MoveReadOffset(buf->ReadableSize()); // 将读偏移向后移动，将可读数据全部读出来

    std::string str = "hello nK!!";
    conn->Send(str.c_str(), str.size());
    conn->Shutdown();
}

int main()
{
    TcpServer server(8080);
    server.SetThreadCount(2);
    server.EnableInactiveRelease(10);

    server.SetConnectedCallback(OnConnected);
    server.SetClosedCallback(OnClosed);
    server.SetMessageCallback(OnMessage);

    server.Start();

    return 0;
}