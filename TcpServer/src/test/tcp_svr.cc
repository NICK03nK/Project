#include "../server.hpp"

void HandleClose(Channel* channel)
{
    std::cout << "close: " << channel->Fd() << std::endl;
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

    std::cout << buf << std::endl;

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

void HandleEvent(Channel* channel)
{
    std::cout << "get a event" << std::endl;
}

void Acceptor(EventLoop* loop, Channel* lst_channel)
{
    int fd = lst_channel->Fd();
    int newfd = accept(fd, NULL, NULL);
    if (newfd < 0) return;

    Channel* channel = new Channel(loop, newfd);
    channel->SetReadCallBack(std::bind(HandleRead, channel));
    channel->SetWriteCallBack(std::bind(HandleWrite, channel));
    channel->SetCloseCallBack(std::bind(HandleClose, channel));
    channel->SetErrorCallBack(std::bind(HandleError, channel));
    channel->SetEventCallBack(std::bind(HandleEvent, channel));

    channel->EnableRead();
}

int main()
{
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