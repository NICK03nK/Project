#include "../server.hpp"

// 管理所有的连接
std::unordered_map<uint64_t, SharedConnection> _conns;
uint64_t conn_id = 0;
EventLoop base_loop;
LoopThreadPool* loop_pool;

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

void NewConnection(int fd)
{
    ++conn_id;

    SharedConnection conn(new Connection(loop_pool->NextLoop(), conn_id, fd));
    conn->SetConnectedCallback(std::bind(OnConnected, std::placeholders::_1));
    conn->SetMessageCallback(std::bind(OnMessage, std::placeholders::_1, std::placeholders::_2));
    conn->SetServerClosedCallback(std::bind(ConnectionDestroy, std::placeholders::_1));

    conn->EnableInactiveRelease(10); // 启动非活跃连接销毁
    conn->Established(); // 就绪初始化

    _conns.insert(std::make_pair(conn_id, conn));

    DBG_LOG("NEW--------------");
}

int main()
{
    loop_pool = new LoopThreadPool(&base_loop);
    loop_pool->SetThreadCount(2);
    loop_pool->Create();

    Acceptor acceptor(&base_loop, 8080);
    acceptor.SetAcceptCallback(std::bind(NewConnection, std::placeholders::_1));
    acceptor.Listen();

    base_loop.Start();

    return 0;
}