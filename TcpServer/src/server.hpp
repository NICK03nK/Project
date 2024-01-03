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
#include <typeinfo>
#include <signal.h>
#include <condition_variable>

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
        fprintf(stdout, "[%p %s %s:%d] " format "\n", (void*)pthread_self(), tmp, __FILE__, __LINE__, ##__VA_ARGS__); \
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
        if (len == 0) return;

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

    // 读取数据并将读偏移向后移动
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
        if (len == 0) return;

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
        _sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
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
            if (errno == EAGAIN || errno == EINTR)
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
        if (ret < 0)
        {
            if (errno == EAGAIN || errno == EINTR)
            {
                return 0; // 表示此次接收没有收到数据
            }

            ERR_LOG("socket send failed");
            return -1;
        }

        return ret; // 返回实际发送数据的字节数
    }

    ssize_t NonBlockSend(const void* buf, size_t len)
    {
        if (len == 0) return 0;

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
    using EventCallback = std::function<void()>;
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
    void SetReadCallback(const EventCallback& cb) { _read_Callback = cb; }

    // 设置文件描述符的写事件回调函数
    void SetWriteCallback(const EventCallback& cb) { _write_Callback = cb; }

    // 设置文件描述符的挂断事件回调函数
    void SetCloseCallback(const EventCallback& cb) { _close_Callback = cb; }

    // 设置文件描述符的错误事件回调函数
    void SetErrorCallback(const EventCallback& cb) { _error_Callback = cb; }

    // 设置文件描述符的任意事件回调函数
    void SetEventCallback(const EventCallback& cb) { _event_Callback = cb; }

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
            if (_event_Callback) _event_Callback();

            if (_read_Callback) _read_Callback();
        }

        // 可能会释放连接的操作事件，一次只处理一个
        if (_revents & EPOLLOUT)
        {
            // 文件描述符触发任意事件都要调用任意事件回调函数
            if (_event_Callback) _event_Callback();
            
            if (_write_Callback) _write_Callback();
        }
        else if (_revents & EPOLLHUP)
        {
            if (_event_Callback) _event_Callback();

            if (_close_Callback) _close_Callback();
        }
        else if (_revents & EPOLLERR)
        {
            if (_event_Callback) _event_Callback();
            
            if (_error_Callback) _error_Callback();
        }
    }

private:
    EventLoop* _loop;

    int _fd;           // 文件描述符 
    uint32_t _events;  // 当前需要监控的事件
    uint32_t _revents; // 当前连接触发的事件

    EventCallback _read_Callback;  // 可读事件被触发的回调函数
    EventCallback _write_Callback; // 可写事件被触发的回调函数
    EventCallback _close_Callback; // 挂断事件被触发的回调函数
    EventCallback _error_Callback; // 错误事件被触发的回调函数
    EventCallback _event_Callback; // 任意事件被触发的回调函数
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
    std::unordered_map<int, Channel*> _channels; // 文件描述符和其对应的channel对象的关联关系
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
        _timer_channel->SetReadCallback(std::bind(&TimerWheel::OnTime, this));
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
        :_thread_id(std::this_thread::get_id())
        , _eventfd(CreateEventFd())
        , _event_channel(new Channel(this, _eventfd))
        , _timer_wheel(this)
    {
        // 设置_eventfd读事件回调函数，读取eventfd事件通知次数
        _event_channel->SetReadCallback(std::bind(&EventLoop::ReadEventFd, this));

        // 启动_eventfd的读事件监控
        _event_channel->EnableRead();
    }

    // 三步走：事件监控-> 就绪事件处理-> 执行任务池中的任务
    void Start()
    {
        while (true)
        {
            // 1.事件监控
            std::vector<Channel*> actives_channels;
            _poller.Poll(&actives_channels);

            // 2.就绪事件处理
            for (const auto &channel : actives_channels)
            {
                channel->HandleEvent();
            }

            // 3.执行线程池中的任务
            RunAllTask();
        }
    }

    // 判断当前线程是否是EventLoop对应的线程
    bool IsInLoop() { return (_thread_id == std::this_thread::get_id()); }

    void AssertInLoop() { assert(_thread_id == std::this_thread::get_id()); }

    // 判断将要执行的任务是否在当前线程中，是则执行；不是则加入任务池中
    void RunInLoop(const Functor& cb)
    {
        // 要执行任务在当前线程中，直接调用执行
        if (IsInLoop()) return cb();

        // 要执行任务不在当前线程中，将任务加入任务池中
        return QueueInLoop(cb);
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
        if (efd < 0)
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
    std::thread::id _thread_id;                // 线程ID
    int _eventfd;                              // 用于唤醒IO事件监控有可能导致的阻塞
    std::unique_ptr<Channel> _event_channel;   // 用于管理_eventfd的事件监控
    Poller _poller;                            // 用于进行所有文件描述符的事件监控
    std::vector<Functor> _tasks;               // 任务池
    std::mutex _mutex;                         // 保证任务池操作的线程安全
    TimerWheel _timer_wheel;                   // 定时器模块
};

class LoopThread
{
public:
    // 创建线程，设定线程入口函数
    LoopThread()
        :_loop(NULL)
        , _thread(&LoopThread::ThreadEntry, this)
    {}

    // 返回当前线程关联的EventLoop对象指针
    EventLoop* GetLoop()
    {
        EventLoop* loop = NULL;

        {
            std::unique_lock<std::mutex> lock(_mutex); // 加锁
            _cond.wait(lock, [&](){ return _loop != NULL; }); // _loop为空就一直阻塞等待

            // 代码执行到这，说明_loop已经实例化出来了
            loop = _loop;
        }

        return loop;
    }

private:
    // 实例化EventLoop对象，并且开始执行EventLoop模块的功能
    void ThreadEntry()
    {
        EventLoop loop; // 实例化一个EventLoop对象

        {
            std::unique_lock<std::mutex> lock(_mutex); // 加锁
            _loop = &loop;

            _cond.notify_all(); // 唤醒有可能阻塞的线程
        }

        _loop->Start();
    }

private:
    std::mutex _mutex;             // 互斥锁
    std::condition_variable _cond; // 条件变量
    EventLoop* _loop;              // EventLoop指针变量，这个变量需要在线程内实例化
    std::thread _thread;           // EventLoop对象对应的线程
};

// Any类
class Any
{
public:
    Any()
        :_content(nullptr)
    {}

    template<class T>
    Any(const T& val)
        :_content(new placeholder<T>(val))
    {}

    Any(const Any& other)
        :_content(other._content ? other._content->clone() : nullptr)
    {}

    ~Any() { delete _content; }

    Any& swap(Any& other)
    {
        std::swap(_content, other._content);
        return *this;
    }

    // 获取Any对象中保存的数据的地址
    template<class T>
    T* get()
    {
        assert(typeid(T) == _content->type()); // 想要获取的数据类型必须和保存的数据类型一致
        return &((placeholder<T>*)_content)->_val;
    }

    template<class T>
    Any& operator=(const T& val)
    {
        Any(val).swap(*this);
        return *this;
    }

    Any& operator=(const Any& other)
    {
        Any(other).swap(*this);
        return *this;
    }

private:
    class holder
    {
    public:
        virtual ~holder() {}

        virtual const std::type_info& type() = 0;

        virtual holder* clone() = 0;
    };

    template <class T>
    class placeholder : public holder
    {
    public:
        placeholder(const T& val)
            :_val(val)
        {}

        // 获取子类对象保存的数据类型
        virtual const std::type_info& type() { return typeid(T); }

        // 根据当前对象克隆出一个新的子类对象
        virtual holder* clone() { return new placeholder(_val); }

    public:
        T _val;
    };

    holder* _content;
};

// Connection类
class Connection;
typedef enum
{
    DISCONNECTED, // 连接关闭状态
    CONNECTING,   // 连接建立成功待处理状态
    CONNECTED,    // 连接建立已完成，各种设置已完成可以通信的状态
    DISCONNECTING // 待关闭状态
} ConnStatu;

using SharedConnection = std::shared_ptr<Connection>; // 使用shared_ptr管理Connection对象，避免对已释放的Connection对象操作，导致内存访问错误
class Connection : public std::enable_shared_from_this<Connection>
{
    using ConnectedCallback = std::function<void(const SharedConnection&)>;
    using MessageCallback = std::function<void(const SharedConnection&, Buffer*)>;
    using ClosedCallback = std::function<void(const SharedConnection&)>;
    using AnyEventCallback = std::function<void(const SharedConnection&)>;
public:
    Connection(EventLoop* loop, uint64_t conn_id, int sockfd)
        :_conn_id(conn_id)
        , _sockfd(sockfd)
        , _enable_inactive_release(false)
        , _loop(loop)
        , _statu(CONNECTING)
        , _socket(_sockfd)
        , _channel(loop, _sockfd)
    {
        _channel.SetCloseCallback(std::bind(&Connection::HandleClose, this));
        _channel.SetEventCallback(std::bind(&Connection::HandleEvent, this));
        _channel.SetReadCallback(std::bind(&Connection::HandleRead, this));
        _channel.SetWriteCallback(std::bind(&Connection::HandleWrite, this));
        _channel.SetErrorCallback(std::bind(&Connection::HandleError, this));
    }

    ~Connection()
    {
        DBG_LOG("release connection: %p", this);
    }

    // 获取连接id
    int Id() { return _conn_id; }

    // 获取管理的文件描述符
    int Fd() { return _sockfd; }

    // 判断是否处于CONNECTED状态
    bool Connected() { return _statu == CONNECTED; }

    // 设置上下文（连接建立完成时进行调用）
    void SetContext(const Any& context) { _context = context; }

    // 获取上下文
    Any* GetContext() { return &_context; }

    void SetConnectedCallback(const ConnectedCallback& cb) { _connected_callback = cb; }

    void SetMessageCallback(const MessageCallback& cb) { _message_callback = cb; }

    void SetClosedCallback(const ClosedCallback& cb) { _closed_callback = cb; }

    void SetAnyEventCallback(const AnyEventCallback& cb) { _event_callback = cb; }

    void SetServerClosedCallback(const ClosedCallback& cb) { _server_closeed_callback = cb; }

    // 连接就绪后，对channel就绪回调设置，启动读事件监控，调用_connected_callback
    void Established()
    {
        _loop->RunInLoop(std::bind(&Connection::EstablishedInLoop, this));
    }

    // 发送数据，将数据放到发送缓冲区，启动写事件监控
    void Send(const char* data, size_t len)
    {
        // 外界传入的data，可能是个临时的空间，我们现在只是把发送操作压入了任务池，有可能并没有被立即执行
        // 因此有可能执行的时候，data指向的空间有可能已经被释放了。
        Buffer buf;
        buf.WriteAndPush(data, len);
        _loop->RunInLoop(std::bind(&Connection::SendInLoop, this, std::move(buf)));
    }

    // 提供给组件使用者的关闭接口（并不实际关闭，需要判断有没有数据在缓冲区中待处理）
    void Shutdown()
    {
        _loop->RunInLoop(std::bind(&Connection::ShutdownInLoop, this));
    }

    void Release()
    {
        _loop->QueueInLoop(std::bind(&Connection::ReleaseInLoop, this));
    }

    // 启动非活跃连接销毁，并定义多长时间无通信就是非活跃，添加定时任务
    void EnableInactiveRelease(int sec)
    {
        _loop->RunInLoop(std::bind(&Connection::EnableInactiveReleaseInLoop, this, sec));
    }

    // 取消非活跃连接销毁
    void CancelInactiveRelease()
    {
        _loop->RunInLoop(std::bind(&Connection::CancelInactiveReleaseInLoop, this));
    }

    // 切换协议（重置上下文以及阶段性处理函数）
    // 这个接口必须在EventLoop线程中立即执行，防备新的事件触发后，处理的时候，切换协议的任务还没有被执行，会导致使用旧有协议去处理数据
    void Upgrade(const Any& context, const ConnectedCallback& conn, const MessageCallback& msg, const ClosedCallback& closed, const AnyEventCallback& event)
    {
        _loop->AssertInLoop();
        _loop->RunInLoop(std::bind(&Connection::UpgradeInLoop, this, context, conn, msg, closed, event));
    }

private:
    // 文件描述符可读事件触发后调用的函数，将接收到的socket数据放到接收缓冲区中，然后调用_message_callback
    void HandleRead()
    {
        // 1.接收socket的数据，放到接收缓冲区中
        char buf[65536] = { 0 };
        ssize_t ret = _socket.NonBlockRecv(buf, 65535);
        if (ret < 0)
        {
            // 出错了，不能直接关闭！
            return ShutdownInLoop();
        }
        // 这里ret=0表示没有读取到数据，并不是连接断开，连接断开返回的是-1

        _in_buffer.WriteAndPush(buf, ret); // 将数据放入输入缓冲区，并将写偏移向后移动

        // 2.调用_message_callback进行业务处理
        if (_in_buffer.ReadableSize() > 0)
        {
            // shared_from_this---从当前对象自身获取自身的shared_ptr管理对象
            return _message_callback(shared_from_this(), &_in_buffer);
        }
    }

    // 文件描述符可写事件触发后调用的函数，将发送缓冲区中的数据进行发送
    void HandleWrite()
    {
        ssize_t ret = _socket.NonBlockSend(_out_buffer.GetReadPos(), _out_buffer.ReadableSize());
        if (ret < 0)
        {
            // 发送错误，关闭连接
            if (_in_buffer.ReadableSize() > 0) // 接收缓冲区中还有数据
            {
                _message_callback(shared_from_this(), &_in_buffer);
            }
            return Release(); // 真正的释放连接
        }

        _out_buffer.MoveReadOffset(ret); // 将读偏移量向后移动
        
        if (_out_buffer.ReadableSize() == 0)
        {
            _channel.DisableWrite(); // 没有数据发送了，关闭文件描述符的写事件监控

            // 如果当前是连接待关闭状态，发送缓冲区中有数据，则发送完数据后再释放连接；发送缓冲区中没有数据，则直接释放
            if (_statu == DISCONNECTING)
            {
                return Release();
            }
        }
    }

    // 文件描述符挂断事件触发后调用的函数
    void HandleClose()
    {
        // 一旦连接触发挂断事件，套接字就什么也干不了了，因此将待处理的数据处理完毕后，关闭连接即可
        if (_in_buffer.ReadableSize() > 0) // 接收缓冲区中还有数据
        {
            _message_callback(shared_from_this(), &_in_buffer);
        }
        return Release();
    }

    // 文件描述符错误事件触发后调用的函数
    void HandleError() { return HandleClose(); }

    // 文件描述符任意事件触发后调用的函数
    void HandleEvent()
    {
        // 1.刷新连接的活跃度（延迟定时销毁任务）
        if (_enable_inactive_release == true) _loop->TimerRefresh(_conn_id);

        // 2.调用组件使用者的任意事件回调
        if (_event_callback) _event_callback(shared_from_this());
    }

    // 连接获取之后，所处的状态下要进行各种设置（启动读事件监控，调用回调函数）
    void EstablishedInLoop()
    {
        // 1.修改连接状态
        assert(_statu == CONNECTING);
        _statu = CONNECTED;

        // 2.启动读事件监控
        _channel.EnableRead();

        // 3.调用回调函数
        if (_connected_callback) _connected_callback(shared_from_this());
    }

    // 该接口并不是实际的发送接口，而只是把数据放到了发送缓冲区，启动可写事件监控
    void SendInLoop(Buffer& buf)
    {
        if (_statu == DISCONNECTED) return;

        // 1.将数据放到输出缓冲区中，并将写偏移向后移动
        _out_buffer.WriteBufferAndPush(buf);

        // 2.启动可写事件监控
        if (_channel.WriteAble() == false)
        {
            _channel.EnableWrite();
        }
    }

    // 该接口并非实际的连接释放操作，接口内判断缓冲区中还有无待处理数据
    void ShutdownInLoop()
    {
        _statu = DISCONNECTING; // 将连接状态设置为半关闭状态

        // 接收缓冲区中还有数据待处理
        if (_in_buffer.ReadableSize() > 0)
        {
            if (_message_callback) _message_callback(shared_from_this(), &_in_buffer);
        }

        // 发送缓冲区中还有数据没发送给对端
        if (_out_buffer.ReadableSize() > 0)
        {
            if (_channel.WriteAble() == false)
            {
                _channel.EnableWrite();
            }
        }

        // 发送缓冲区中没有待发送数据，直接关闭连接
        if (_out_buffer.ReadableSize() == 0)
        {
            Release();
        }
    }

    // 实际的释放连接接口
    void ReleaseInLoop()
    {
        // 1.修改呢连接状态，将其设置为DISCONNECTED
        _statu = DISCONNECTED;

        // 2.移除连接的事件监控
        _channel.Remove();

        // 3.关闭文件描述符
        _socket.Close();

        // 4.若当前定时器队列中还有定时销毁任务，则取消任务
        if (_loop->HasTimer(_conn_id)) CancelInactiveReleaseInLoop();

        // 5.调用关闭回调函数
        if (_closed_callback) _closed_callback(shared_from_this());
        // 移除服务器内部管理的连接信息
        if (_server_closeed_callback) _server_closeed_callback(shared_from_this());
    }

    // 启动非活跃连接超时释放
    void EnableInactiveReleaseInLoop(int sec)
    {
        // 1.将判断标志_enable_inactive_release设置为true
        _enable_inactive_release = true;

        // 2.若当前已存在定时销毁任务，则刷新延迟即可
        if (_loop->HasTimer(_conn_id)) return _loop->TimerRefresh(_conn_id);

        // 3.若不存在定时销毁任务，则新增
        _loop->TimerAdd(_conn_id, sec, std::bind(&Connection::Release, this));
    }

    // 关闭非活跃连接超时释放
    void CancelInactiveReleaseInLoop()
    {
        _enable_inactive_release = false;

        if (_loop->HasTimer(_conn_id)) _loop->TimerCancel(_conn_id);
    }

    void UpgradeInLoop(const Any& context, const ConnectedCallback& conn, const MessageCallback& msg, const ClosedCallback& closed, const AnyEventCallback& event)
    {
        _context = context;
        _connected_callback = conn;
        _message_callback = msg;
        _closed_callback = closed;
        _event_callback = event;
    }

private:
    uint64_t _conn_id;             // 标识连接的唯一id，便于连接的管理和查找
    // uint64_t _timer_id;         // 定时器id，必须是唯一的（为了简化操作，使用_conn_id作为定时器id）
    int _sockfd;                   // 连接关联的文件描述符
    bool _enable_inactive_release; // 连接是否启动非活跃销毁的标志
    EventLoop* _loop;              // 连接所关联的一个EventLoop
    ConnStatu _statu;              // 连接状态
    Socket _socket;                // 套接字操作管理
    Channel _channel;              // 连接的事件管理
    Buffer _in_buffer;             // 输入缓冲区----存放从socket中读取到的数据
    Buffer _out_buffer;            // 输出缓冲区----存放要发送给对端的数据
    Any _context;                  // 请求的接收处理上下文

    ConnectedCallback _connected_callback;
    MessageCallback _message_callback;
    ClosedCallback _closed_callback;
    AnyEventCallback _event_callback;
    // 组件内的连接关闭回调--组件内设置的，因为服务器组件内会把所有的连接
    // 管理起来，一旦某个连接要关闭，就应该从管理的地方移除掉自己的信息
    ClosedCallback _server_closeed_callback;
};

// Acceptor类
class Acceptor
{
    using AcceptCallback = std::function<void(int)>;
public:
    Acceptor(EventLoop* loop, uint16_t port)
        :_socket(CreateServer(port))
        , _loop(loop)
        , _channel(loop, _socket.Fd())
    {
        _channel.SetReadCallback(std::bind(&Acceptor::HandleRead, this));
    }

    void Listen() { _channel.EnableRead(); }

    void SetAcceptCallback(const AcceptCallback& cb) { _accept_callback = cb; }

private:
    void HandleRead()
    {
        int newfd = _socket.Accept();
        if (newfd < 0)
        {
            return;
        }

        if (_accept_callback) _accept_callback(newfd);
    }

    int CreateServer(uint16_t port)
    {
        bool ret = _socket.CreateServer(port);
        assert(ret == true);
        
        return _socket.Fd();
    }

private:
    Socket _socket;   // 用于创建监听套接字
    EventLoop* _loop; // 用于对监听套接字进行事件监控
    Channel _channel; // 用于对监听套接字进行事件管理

    AcceptCallback _accept_callback;
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

class NetWork {
    public:
        NetWork() {
            DBG_LOG("SIGPIPE INIT");
            signal(SIGPIPE, SIG_IGN);
        }
};
static NetWork nw;