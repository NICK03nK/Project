#include "../server.hpp"

class EchoServer
{
public:
    EchoServer(uint16_t port)
        :_server(port)
    {
        _server.SetThreadCount(2);
        _server.EnableInactiveRelease(10);

        _server.SetConnectedCallback(std::bind(&EchoServer::OnConnected, this, std::placeholders::_1));
        _server.SetClosedCallback(std::bind(&EchoServer::OnClosed, this, std::placeholders::_1));
        _server.SetMessageCallback(std::bind(&EchoServer::OnMessage, this, std::placeholders::_1, std::placeholders::_2));
    }

    void Start() { _server.Start(); }

private:
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
        conn->Send(buf->GetReadPos(), buf->ReadableSize());
        buf->MoveReadOffset(buf->ReadableSize()); // 将读偏移向后移动，将可读数据全部读出来
        conn->Shutdown();
    }

private:
    TcpServer _server;
};