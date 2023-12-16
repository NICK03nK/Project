#include "../server.hpp"

void HandleClose(Channel* channel)
{
    DBG_LOG("close fd: %d", channel->Fd());
    channel->Remove(); // 移除监控
    delete channel; // 将先前new出来的Channel对象释放
}

void HandleRead(Channel* channel)
{
    int fd = channel->Fd();
    char buf[1024] = { 0 };

    int ret = recv(fd, buf, 1023, 0);
    if (ret <= 0)
    {
        return HandleClose(channel);
    }

    DBG_LOG("%s", buf);

    channel->EnableWrite(); // 启动可写事件
}

void HandleWrite(Channel* channel)
{
    int fd = channel->Fd();
    const char* data = "hi";
    int ret = send(fd, data, strlen(data), 0);
    if (ret < 0)
    {
        return HandleClose(channel);
    }

    channel->DisableWrite(); // 关闭写监控
}

void HandleError(Channel* channel)
{
    return HandleClose(channel);
}

void HandleEvent(EventLoop* loop, Channel* channel, uint64_t timerid)
{
    loop->TimerRefresh(timerid);
}

void Acceptor(EventLoop* loop, Channel* lst_channel)
{
    int fd = lst_channel->Fd();
    int newfd = accept(fd, NULL, NULL);
    if (newfd < 0) return;

    uint64_t timerid = rand() % 10000;

    Channel* channel = new Channel(loop, newfd);
    channel->SetReadCallBack(std::bind(HandleRead, channel));
    channel->SetWriteCallBack(std::bind(HandleWrite, channel));
    channel->SetCloseCallBack(std::bind(HandleClose, channel));
    channel->SetErrorCallBack(std::bind(HandleError, channel));
    channel->SetEventCallBack(std::bind(HandleEvent, loop, channel, timerid));

    // 非活跃连接的超时释放操作
    loop->TimerAdd(timerid, 10, std::bind(HandleClose, channel));

    channel->EnableRead();
}

int main()
{
    srand(time(NULL));

    EventLoop loop;
    
    Socket lst_sock;
    lst_sock.CreateServer(8080);

    // 为监听套接字创建一个Channel对象，进行事件的管理以及事件的处理
    Channel channel(&loop, lst_sock.Fd());
    channel.SetReadCallBack(std::bind(Acceptor, &loop, &channel));
    channel.EnableRead(); // 启动监听套接字读事件监控

    while (true)
    {
        loop.Start();
    }
    lst_sock.Close();

    return 0;
}