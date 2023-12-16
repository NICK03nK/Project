#include <iostream>
#include <vector>
#include <cassert>
#include <string>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <functional>
#include <sys/epoll.h>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <sys/eventfd.h>
#include <memory>
#include <sys/timerfd.h>

// 日志宏
#define INF 0
#define DBG 1
#define ERR 2
#define LOG_LEVEL INF
#define LOG(level, format, ...)                                                             \
    do                                                                                      \
    {                                                                                       \
        if (level < LOG_LEVEL) break;                                                       \
        time_t t = time(NULL);                                                              \
        struct tm *ltm = localtime(&t);                                                     \
        char tmp[32] = {0};                                                                 \
        strftime(tmp, 31, "%H:%M:%S", ltm);                                                 \
        fprintf(stdout, "[%s %s:%d] " format "\n", tmp, __FILE__, __LINE__, ##__VA_ARGS__); \
    } while (0)

#define INF_LOG(format, ...) LOG(INF, format, ##__VA_ARGS__)
#define DBG_LOG(format, ...) LOG(DBG, format, ##__VA_ARGS__)
#define ERR_LOG(format, ...) LOG(ERR, format, ##__VA_ARGS__)

// Buffer类
#define BUFFER_DEFAULT_SIZE 1024
class Buffer
{
public:
    Buffer()
        :_buffer(BUFFER_DEFAULT_SIZE)
        , _read_index(0)
        , _write_index(0)
    {}

    // 返回缓冲区的起始地址
    char* Begin() { return &(*_buffer.begin()); }

    // 获取当前读取位置起始地址
    char* GetReadPos() { return Begin() + _read_index; }

    // 获取当前写入位置起始地址（缓冲区起始地址加上写偏移量）
    char* GetWritePos() { return Begin() + _write_index; }

    // 获取缓冲区头部空闲空间大小--读偏移之前的空闲空间
    uint64_t HeadIdleSize() { return _read_index; }

    // 获取缓冲区末尾空闲空间大小--写偏移之后的空闲空间
    uint64_t TailIdleSize() { return _buffer.size() - _write_index; }

    // 获取可读数据大小
    uint64_t ReadableSize() { return _write_index - _read_index; }

    // 将读偏移向后移动
    void MoveReadOffset(uint64_t len)
    {
        assert(len <= ReadableSize()); // 向后移动的大小必须小于可读数据的大小
        _read_index += len;
    }

    // 将写偏移向后移动
    void MoveWriteOffset(uint64_t len)
    {
        assert(len <= TailIdleSize()); // 向后移动的大小必须小于缓冲区末尾空闲空间的大小
        _write_index += len;
    }

    // 确保可写空间足够（整体的空闲空间够存数据就将数据移动，否则扩容）
    void EnsureWriteSpace(uint64_t len)
    {
        // 若末尾空闲空间足够，则直接返回
        if (TailIdleSize() >= len) return;

        // 若末尾空闲空间不够，则判断末尾空闲空间加上头部空闲空间是否足够
        // 足够则将可读数据移动到缓冲区的起始位置，给后面留出完整的空间来写入数据
        if (HeadIdleSize() + TailIdleSize() >= len)
        {
            // 将可读数据移动到缓冲区起始位置
            uint64_t readsize = ReadableSize(); // 保存缓冲区当前可读数据的大小
            std::copy(GetReadPos(), GetReadPos() + readsize, Begin()); // 将可读数据拷贝到缓冲区起始位置
            _read_index = 0; // 将读偏移归零
            _write_index = readsize; // 将写偏移置为可读数据大小
        }
        else // 总体空间不够，则进行扩容，无需移动数据，直接在写偏移之后扩容足够的空间即可
        {
            _buffer.resize(_write_index + len);
        }
    }

    // 读取数据
    void Read(void* buffer, uint64_t len)
    {
        // 读取数据的大小必须小于可读数据大小
        assert(len <= ReadableSize());
        std::copy(GetReadPos(), GetReadPos() + len, (char*)buffer);
    }

    // 读取数据并将du偏移向后移动
    void ReadAndPop(void* buffer, uint64_t len)
    {
        Read(buffer, len);
        MoveReadOffset(len);
    }

    // 将数据作为字符串读取
    std::string ReadAsString(uint64_t len)
    {
        // 读取数据的大小必须小于可读数据大小
        assert(len <= ReadableSize());

        std::string str;
        str.resize(len);

        Read(&str[0], len);

        return str;
    }

    std::string ReadAsStringAndPop(uint64_t len)
    {
        std::string str = ReadAsString(len);
        MoveReadOffset(len);

        return str;
    }

    // 写入数据
    void Write(const void* data, uint64_t len)
    {
        // 1.保证缓冲区有足够的可写空间
        EnsureWriteSpace(len);

        // 2.拷贝数据
        const char* d = (const char*)data;
        std::copy(d, d + len, GetWritePos());
    }

    void WriteAndPush(const void* data, uint64_t len)
    {
        Write(data, len);
        MoveWriteOffset(len);
    }

    // 往缓冲区中写入字符串
    void WriteString(const std::string& data)
    {
        return Write(data.c_str(), data.size());
    }

    void WriteStringAndPush(const std::string& data)
    {
        WriteString(data);
        MoveWriteOffset(data.size());
    }

    // 往缓冲区中写入另一个缓冲区的数据
    void WriteBuffer(Buffer& data)
    {
        return Write(data.GetReadPos(), data.ReadableSize());
    }

    void WriteBufferAndPush(Buffer& data)
    {
        WriteBuffer(data);
        MoveWriteOffset(data.ReadableSize());
    }

    char* FindCRLF()
    {
        char* pos = (char*)memchr(GetReadPos(), '\n', ReadableSize());
        return pos;
    }

    std::string GetLine()
    {
        char* pos = FindCRLF();
        if (pos == NULL) return "";

        return ReadAsString(pos - GetReadPos() + 1); // +1是为了把'\n'也取出来
    }

    std::string GetLineAndPop()
    {
        std::string str = GetLine();
        MoveReadOffset(str.size());
        
        return str;
    }

    // 清空缓冲区
    void Clear()
    {
        // 只需要将两个偏移量归零即可
        _read_index = 0;
        _write_index = 0;
    }

private:
    std::vector<char> _buffer; // 使用vector进行内存管理
    uint64_t _read_index;      // 读偏移
    uint64_t _write_index;     // 写偏移
};

// Socket类
#define MAX_LISTEN 1024
class Socket
{
public:
    Socket() :_sockfd(-1) {}

    Socket(int sockfd) :_sockfd(sockfd) {}

    ~Socket() { Close(); }

    // 创建套接字
    bool Create()
    {
        // 调用socket()
        _sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (_sockfd == -1)
        {
            ERR_LOG("create socket failed");
            return false;
        }
        
        return true;
    }

    // 绑定地址信息
    bool Bind(const std::string& ip, uint16_t port)
    {
        // 定义一个struct sockaddr_in对象
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(ip.c_str());
        socklen_t len = sizeof(struct sockaddr_in);

        // 调用bind()
        int ret = bind(_sockfd, (struct sockaddr*)&addr, len);
        if (ret == -1)
        {
            ERR_LOG("bind address failed");
            return false;
        }

        return true;
    }

    // 开始监听
    bool Listen(int backlog = MAX_LISTEN)
    {
        // 调用listen()
        int ret = listen(_sockfd, backlog);
        if (ret == -1)
        {
            ERR_LOG("socket listen failed");
            return false;
        }

        return true;
    }

    // 向服务器发起连接（传入服务器的ip和port）
    bool Connect(const std::string& ip, uint16_t port)
    {
        // 定义一个struct sockaddr_in对象
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(ip.c_str());
        socklen_t len = sizeof(struct sockaddr_in);

        // 调用connect()
        int ret = connect(_sockfd, (struct sockaddr*)&addr, len);
        if (ret == -1)
        {
            ERR_LOG("connect server failed");
            return false;
        }

        return true;
    }

    // 获取新连接
    int Accept()
    {
        // 调用accept()
        int ret = accept(_sockfd, NULL, NULL);
        if (ret == -1)
        {
            ERR_LOG("socket accept failed");
            return -1;
        }

        return ret;
    }

    // 接收数据
    ssize_t Recv(void* buf, size_t len, int flag = 0) // flag = 0默认阻塞
    {
        // 调用recv()
        ssize_t ret = recv(_sockfd, buf, len, flag);
        if (ret <= 0)
        {
            // ret = EAGAIN 表示当前socket的接收缓冲区中没有数据，在非阻塞的情况下才会有这个返回值
            // ret = EINTR 表示当前socket的阻塞等待被信号打断了
            if (ret == EAGAIN || ret == EINTR)
            {
                return 0; // 表示此次接收没有收到数据
            }

            ERR_LOG("socket receive failed");
            return -1;
        }

        return ret; // 返回实际接收到数据的字节数
    }

    ssize_t NonBlockRecv(void* buf, size_t len)
    {
        return Recv(buf, len, MSG_DONTWAIT); // MSG_DONTWAIT表示为非阻塞接收
    }

    // 发送数据
    ssize_t Send(const void* buf, size_t len, int flag = 0) // flag = 0默认阻塞
    {
        // 调用send()
        ssize_t ret = send(_sockfd, buf, len, flag);
        if (ret == -1)
        {
            ERR_LOG("socket send failed");
            return -1;
        }

        return ret; // 返回实际发送数据的字节数
    }

    ssize_t NonBlockSend(const void* buf, size_t len)
    {
        return Send(buf, len, MSG_DONTWAIT); // MSG_DONTWAIT表示为非阻塞发送
    }
    
    // 关闭套接字
    void Close()
    {
        if (_sockfd != -1)
        {
            close(_sockfd);
            _sockfd = -1;
        }
    }

    // 创建一个服务端连接
    bool CreateServer(uint16_t port, const std::string& ip = "0.0.0.0", bool block_flag = false)
    {
        // 1.创建套接字，2.设置非阻塞，3.绑定ip和port，4.开始监听，5.启动地址重用
        if (Create() == false) return false;
        if (block_flag) NonBlock();
        if (Bind(ip, port) == false) return false;
        if (Listen() == false) return false;
        ReuseAddress();

        return true;
    }

    // 创建一个客户端连接
    bool CreateClient(uint16_t port, const std::string& ip)
    {
        // 1.创建套接字，2.连接服务器
        if (Create() == false) return false;
        if (Connect(ip, port) == false) return false;

        return true;
    }

    // 设置套接字选项——开启地址端口重用
    void ReuseAddress()
    {
        // 调用setsockopt()
        int opt = 1;
        setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, (void*)&opt, sizeof(opt));
        opt = 1;
        setsockopt(_sockfd, SOL_SOCKET, SO_REUSEPORT, (void*)&opt, sizeof(opt));
    }

    // 设置套接字阻塞属性——非阻塞
    void NonBlock()
    {
        int flag = fcntl(_sockfd, F_GETFL);
        fcntl(_sockfd, F_SETFL, flag | O_NONBLOCK);
    }

    int Fd() { return _sockfd; }

private:
    int _sockfd;
};

// Channel类
class Poller; // Poller类的声明，让Channel能够使用Poller类
class EventLoop; // EventLoop类的声明，让Channel能够使用EventLoop类
class Channel
{
    using EventCallBack = std::function<void()>;
public:
    Channel(EventLoop* loop, int fd)
        :_loop(loop)
        , _fd(fd)
        , _events(0)
        , _revents(0)
    {}

    int Fd() { return _fd; }

    // 获取文件描述符想要监控的事件
    uint32_t Events() { return _events; } 

    // 设置实际就绪的事件
    void SetREvents(uint32_t events) { _revents = events; }  // EventLoop模块会调用该函数，EventLoop模块会将文件描述符实际触发的事件设置进_revents中

    // 设置文件描述符的读事件回调函数
    void SetReadCallBack(const EventCallBack& cb) { _read_callback = cb; }

    // 设置文件描述符的写事件回调函数
    void SetWriteCallBack(const EventCallBack& cb) { _write_callback = cb; }

    // 设置文件描述符的挂断事件回调函数
    void SetCloseCallBack(const EventCallBack& cb) { _close_callback = cb; }

    // 设置文件描述符的错误事件回调函数
    void SetErrorCallBack(const EventCallBack& cb) { _error_callback = cb; }

    // 设置文件描述符的任意事件回调函数
    void SetEventCallBack(const EventCallBack& cb) { _event_callback = cb; }

    // 判断当前描述符是否监控了可读事件
    bool ReadAble() { return (_events & EPOLLIN); }

    // 判断当前描述符是否监控了可写事件
    bool WriteAble() { return (_events & EPOLLOUT); }

    // 启动描述符读事件监控
    void EnableRead()
    {
        _events |= EPOLLIN;

        Update();
    }

    // 启动描述符写事件监控
    void EnableWrite()
    {
        _events |= EPOLLOUT;

        Update();
    }

    // 解除描述符读事件监控
    void DisableRead()
    {
        _events &= ~EPOLLIN;

        Update();
    }

    // 解除描述符写事件监控
    void DisableWrite()
    {
        _events &= ~EPOLLOUT;

        Update();
    }

    // 解除描述符所有事件监控
    void DisableAll() { _events = 0; }

    void Update();

    // 将描述符从epoll模型中移除监控
    void Remove();

    // 事件处理总函数
    void HandleEvent()
    {
        if ((_revents & EPOLLIN) || (_revents & EPOLLRDHUP) || (_revents & EPOLLPRI))
        {
            // 文件描述符触发任意事件都要调用任意事件回调函数
            if (_event_callback) _event_callback();

            if (_read_callback) _read_callback();
        }

        // 可能会释放连接的操作事件，一次只处理一个
        if (_revents & EPOLLOUT)
        {
            // 文件描述符触发任意事件都要调用任意事件回调函数
            if (_event_callback) _event_callback();
            
            if (_write_callback) _write_callback();
        }
        else if (_revents & EPOLLHUP)
        {
            if (_event_callback) _event_callback();

            if (_close_callback) _close_callback();
        }
        else if (_revents & EPOLLERR)
        {
            if (_event_callback) _event_callback();
            
            if (_error_callback) _error_callback();
        }
    }

private:
    EventLoop* _loop;

    int _fd;           // 文件描述符 
    uint32_t _events;  // 当前需要监控的事件
    uint32_t _revents; // 当前连接触发的事件

    EventCallBack _read_callback;  // 可读事件被触发的回调函数
    EventCallBack _write_callback; // 可写事件被触发的回调函数
    EventCallBack _close_callback; // 挂断事件被触发的回调函数
    EventCallBack _error_callback; // 错误事件被触发的回调函数
    EventCallBack _event_callback; // 任意事件被触发的回调函数
};

// Poller类
#define MAX_EPOLLEVENTS 1024
class Poller
{
public:
    Poller()
    {
        _epfd = epoll_create(MAX_EPOLLEVENTS);
        if (_epfd == -1)
        {
            ERR_LOG("epoll create failed");
            abort(); // 退出程序
        }
    }

    // 添加或修改文件描述符的监控事件
    void UpdateEvent(Channel* channel)
    {
        // 先判断是否在hash中管理，不在则添加；在则修改
        bool ret = HasChannel(channel);
        if (ret == false)
        {
            // 将该channel和其对应的文件描述符添加到_channels中
            _channels.insert(std::make_pair(channel->Fd(), channel));

            return Update(channel, EPOLL_CTL_ADD);
        }

        return Update(channel, EPOLL_CTL_MOD);
    }

    // 移除文件描述符的事件监控
    void RemoveEvent(Channel* channel)
    {
        // 从hash中移除
        auto it = _channels.find(channel->Fd());
        if (it != _channels.end())
        {
            _channels.erase(it);
        }

        // 删除监控事件
        Update(channel, EPOLL_CTL_DEL);
    }

    // 开始监控，返回活跃连接
    void Poll(std::vector<Channel*>* active)
    {
        // 调用epoll_wait()
        int nfds = epoll_wait(_epfd, _evs, MAX_EPOLLEVENTS, -1);
        if (nfds == -1)
        {
            if (errno == EINTR) return;

            ERR_LOG("epoll wait error:%s", strerror(errno));
            abort(); // 退出程序
        }

        for (int i = 0; i < nfds; ++i)
        {
            auto it = _channels.find(_evs[i].data.fd);
            assert(it != _channels.end());

            it->second->SetREvents(_evs[i].events); // 将实际就绪事件设置到对应文件描述符的Channel对象中
            active->push_back(it->second);
        }
    }

private:
    // 对epoll直接操作（增，删，改）
    void Update(Channel* channel, int op)
    {
        // 调用epoll_ctl()
        int fd = channel->Fd();
        struct epoll_event ev;
        ev.data.fd = fd;
        ev.events = channel->Events();

        int ret = epoll_ctl(_epfd, op, fd, &ev);
        if (ret == -1)
        {
            ERR_LOG("epoll_ctl failed");
        }
    }

    // 判断一个Channel对象是否添加了事件监控（是否在hash中管理）
    bool HasChannel(Channel* channel)
    {
        auto it = _channels.find(channel->Fd());
        if (it == _channels.end())
        {
            return false;
        }

        return true;
    }

private:
    int _epfd;
    struct epoll_event _evs[MAX_EPOLLEVENTS];
    std::unordered_map<int, Channel*> _channels;
};

using TaskFunc = std::function<void()>;
using ReleaseFunc = std::function<void()>;

// 定时器任务类
class TimerTask
{
public:
    TimerTask(uint64_t id, uint32_t delay, const TaskFunc& cb)
        :_id(id)
        , _timeout(delay)
        , _canceled(false)
        , _task_cb(cb)
    {}

    void SetRelease(const ReleaseFunc& cb)
    {
        _release = cb;
    }

    uint32_t DelayTime()
    {
        return _timeout;
    }

    void Cancel()
    {
        _canceled = true;
    }

    ~TimerTask()
    {
        if (_canceled == false) _task_cb();
        _release();
    }

private:
    uint64_t _id;         // 定时任务对象id
    uint32_t _timeout;    // 定时任务的超时时间
    bool _canceled;       // true--取消定时任务，false--不取消
    TaskFunc _task_cb;    // 定时器要执行的定时任务
    ReleaseFunc _release; // 用于删除TimerWheel中保存的定时任务对象信息(删除unordered_map中的元素)
};

// 时间轮类
class TimerWheel
{
public:
    TimerWheel(EventLoop* loop)
        :_tick(0)
        , _capacity(60)
        , _wheel(_capacity)
        , _loop(loop)
        , _timerfd(CreateTimerFd())
        , _timer_channel(new Channel(_loop, _timerfd))
    {
        _timer_channel->SetReadCallBack(std::bind(&TimerWheel::OnTime, this));
        _timer_channel->EnableRead(); // 启动定时器文件描述符的读事件监控
    }

    bool HasTimer(uint64_t id) // 该接口存在线程安全问题！不能被外界使用者调用，只能在模块内，在对应的EventLoop线程内执行
    {
        auto it = _timers.find(id);
        if (it == _timers.end()) return false;

        return true;
    }

    void TimerAdd(uint64_t id, uint32_t delay, const TaskFunc &cb);

    void TimerRefresh(uint64_t id);

    void TimerCancel(uint64_t id);

private:
    void RemoveTimer(uint64_t id)
    {
        auto it = _timers.find(id);
        if (it != _timers.end())
        {
            _timers.erase(it);
        }
    }

    static int CreateTimerFd()
    {
        int timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
        if (timerfd < 0)
        {
            ERR_LOG("timerfd create failede");
            abort();
        }

        struct itimerspec itime;

        // 第一次超时时间为1s后
        itime.it_value.tv_sec = 1;
        itime.it_value.tv_nsec = 0;

        // 第一次超时后，每次超时的间隔时间为1s
        itime.it_interval.tv_sec = 1;
        itime.it_interval.tv_nsec = 0;

        timerfd_settime(timerfd, 0, &itime, nullptr);

        return timerfd;
    }

    void ReadTimerFd()
    {
        uint64_t times;
        int ret = read(_timerfd, &times, 8);
        if (ret < 0)
        {
            ERR_LOG("read timerfd failed");
            abort();
        }
    }

    // 时间轮定时任务执行函数(该函数每秒执行一次)
    void RunTimerTask()
    {
        _tick = (_tick + 1) % _capacity;
        _wheel[_tick].clear(); // 清空指针位置上的数组，将所有管理定时任务对象的shared_ptr给释放掉
    }

    void OnTime()
    {
        ReadTimerFd();
        RunTimerTask();
    }

    // 添加定时任务
    void TimerAddInLoop(uint64_t id, uint32_t delay, const TaskFunc& cb)
    {
        SharedTask st(new TimerTask(id, delay, cb));
        st->SetRelease(std::bind(&TimerWheel::RemoveTimer, this, id));
        _timers[id] = WeakTask(st);

        // 将定时任务添加到时间轮中
        int pos = (_tick + delay) % _capacity;
        _wheel[pos].push_back(st);
    }

    // 刷新/延迟定时任务
    void TimerRefreshInLoop(uint64_t id)
    {
        // 通过定时器对象的weak_ptr构造一个shared_ptr，再添加到时间轮中
        auto it = _timers.find(id);
        if (it == _timers.end()) return;

        SharedTask st = it->second.lock();
        int delay = st->DelayTime();
        int pos = (_tick + delay) % _capacity;
        _wheel[pos].push_back(st);
    }

    void TimerCancelInLoop(uint64_t id)
    {
        auto it = _timers.find(id);
        if (it == _timers.end()) return;

        SharedTask st = it->second.lock();
        if (st) st->Cancel();
    }

private:
    using SharedTask = std::shared_ptr<TimerTask>;
    using WeakTask = std::weak_ptr<TimerTask>;

    int _tick;                                      // 当前的秒针(走到哪里释放哪里，释放哪里就相当于执行哪里的定时任务)
    int _capacity;                                  // 轮的最大数量（最大延迟时间）
    std::vector<std::vector<SharedTask>> _wheel;    // 时间轮
    std::unordered_map<uint64_t, WeakTask> _timers; // 将定时任务id和管理定时任务的weak_ptr绑定

    EventLoop* _loop;
    int _timerfd; // 定时器文件描述符
    std::unique_ptr<Channel> _timer_channel;
};

// EventLoop类
class EventLoop
{
    using Functor = std::function<void()>;
public:
    EventLoop()
        :_id(std::this_thread::get_id())
        , _eventfd(CreateEventFd())
        , _event_channel(new Channel(this, _eventfd))
        , _timer_wheel(this)
    {
        // 设置_eventfd读事件回调函数，读取eventfd事件通知次数
        _event_channel->SetReadCallBack(std::bind(&EventLoop::ReadEventFd, this));

        // 启动_eventfd的读事件监控
        _event_channel->EnableRead();
    }

    // 三步走：事件监控-> 就绪事件处理-> 执行任务池中的任务
    void Start()
    {
        while (true)
        {
            // 1.事件监控
            std::vector<Channel *> actives_channels;
            _poller.Poll(&actives_channels);

            // 2.就绪事件处理
            for (const auto &channel : actives_channels)
            {
                channel->HandleEvent();
            }
        }

        // 3.执行线程池中的任务
        RunAllTask();
    }

    // 判断当前线程是否是EventLoop对应的线程
    bool IsInLoop() { return _id == std::this_thread::get_id(); }

    // 判断将要执行的任务是否在当前线程中，是则执行；不是则加入任务池中
    void RunInLoop(const Functor& cb)
    {
        // 要执行任务在当前线程中，直接调用执行
        if (IsInLoop()) cb();

        // 要执行任务不在当前线程中，将任务加入任务池中
        QueueInLoop(cb);
    }

    // 将操作加入任务池中
    void QueueInLoop(const Functor& cb)
    {
        {
            std::unique_lock<std::mutex> _lock(_mutex);

            _tasks.push_back(cb);
        }

        // 唤醒IO事件监控有可能导致的阻塞
        WakeUpEventFd();
    }

    // 添加或修改文件描述符的监控事件
    void UpdateEvent(Channel* channel) { return _poller.UpdateEvent(channel); }

    // 移除文件描述符的事件监控
    void RemoveEvent(Channel* channel) { return _poller.RemoveEvent(channel); }

    void TimerAdd(uint64_t id, uint32_t delay, const TaskFunc& cb) { return _timer_wheel.TimerAdd(id, delay, cb); }

    void TimerRefresh(uint64_t id) { return _timer_wheel.TimerRefresh(id); }

    void TimerCancel(uint64_t id) { return _timer_wheel.TimerCancel(id); }

    bool HasTimer(uint64_t id) { return _timer_wheel.HasTimer(id); }

private:
    // 执行任务池中的所有任务
    void RunAllTask()
    {
        std::vector<Functor> functors;
        {
            std::unique_lock<std::mutex> _lock(_mutex);
             
            _tasks.swap(functors);
        }
        
        for (const auto& f : functors)
        {
            f();
        }
    }

    static int CreateEventFd()
    {
        int efd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
        if (efd == -1)
        {
            ERR_LOG("create eventfd failed");
            abort(); // 让程序异常退出
        }

        return efd;
    }

    void ReadEventFd()
    {
        uint64_t res = 0;
        int ret = read(_eventfd, &res, sizeof(res));
        if (ret < 0)
        {
            // EINTR -- 表示被信号打断    EAGAIN -- 表示无数据可读
            if (errno == EINTR || errno == EAGAIN)
            {
                return;
            }

            ERR_LOG("read eventfd failed");
            abort();
        }
    }

    // 本质是向eventfd中写入数据
    void WakeUpEventFd()
    {
        uint64_t val = 1;
        int ret = write(_eventfd, &val, sizeof(val));
        if (ret < 0)
        {
            // EINTR -- 表示被信号打断
            if (errno == EINTR)
            {
                return;
            }

            ERR_LOG("write eventfd failed");
            abort();
        }
    }

private:
    std::thread::id _id;                       // 线程ID
    int _eventfd;                              // 用于唤醒IO事件监控有可能导致的阻塞
    std::unique_ptr<Channel> _event_channel;   // 用于管理_eventfd的事件监控
    Poller _poller;                            // 用于进行所有文件描述符的事件监控
    std::vector<Functor> _tasks;               // 任务池
    std::mutex _mutex;                         // 保证任务池操作的线程安全
    TimerWheel _timer_wheel;                   // 定时器模块
};

// Channel类中的两个成员函数
void Channel::Update() { return _loop->UpdateEvent(this); }
void Channel::Remove() { return _loop->RemoveEvent(this); }

// TimerWheel类中的三个成员函数
void TimerWheel::TimerAdd(uint64_t id, uint32_t delay, const TaskFunc &cb)
{
    _loop->RunInLoop(std::bind(&TimerWheel::TimerAddInLoop, this, id, delay, cb));
}
void TimerWheel::TimerRefresh(uint64_t id)
{
    _loop->RunInLoop(std::bind(&TimerWheel::TimerRefreshInLoop, this, id));
}
void TimerWheel::TimerCancel(uint64_t id)
{
    _loop->RunInLoop(std::bind(&TimerWheel::TimerCancelInLoop, this, id));
}