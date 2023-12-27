#include "../server.hpp"

// 管理所有的连接
std::unordered_map<uint64_t, SharedConnection> _conns;

uint64_t conn_id = 0;

void ConnectionDestroy(const SharedConnection& conn)
{
    _conns.erase(conn->Id());
}

void OnConnected(const SharedConnection& conn)
{
    DBG_LOG("new connection: %p", conn.get());
}

void OnMessage(const SharedConnection& conn, Buffer* buf)
{
    DBG_LOG("%s", buf->GetReadPos());
    buf->MoveReadOffset(buf->ReadableSize()); // 将读偏移向后移动，将可读数据全部读出来

    std::string str = "hello nK!!";
    conn->Send(str.c_str(), str.size());
    conn->Shutdown();
}

void Acceptor(EventLoop* loop, Channel* lst_channel)
{
    int fd = lst_channel->Fd();
    int newfd = accept(fd, NULL, NULL);
    if (newfd < 0) return;

    ++conn_id;

    SharedConnection conn(new Connection(loop, conn_id, newfd));
    conn->SetConnectedCallback(std::bind(OnConnected, std::placeholders::_1));
    conn->SetMessageCallback(std::bind(OnMessage, std::placeholders::_1, std::placeholders::_2));
    conn->SetServerClosedCallback(std::bind(ConnectionDestroy, std::placeholders::_1));

    conn->EnableInactiveRelease(10); // 启动非活跃连接销毁
    conn->Established(); // 就绪初始化

    _conns.insert(std::make_pair(conn_id, conn));
}

int main()
{
    EventLoop loop;
    
    Socket lst_sock;
    lst_sock.CreateServer(8080);

    // 为监听套接字创建一个Channel对象，进行事件的管理以及事件的处理
    Channel channel(&loop, lst_sock.Fd());
    channel.SetReadCallback(std::bind(Acceptor, &loop, &channel));
    channel.EnableRead(); // 启动监听套接字读事件监控

    while (true)
    {
        loop.Start();
    }
    lst_sock.Close();

    return 0;
}