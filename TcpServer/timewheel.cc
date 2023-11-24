#include <iostream>
#include <functional>
#include <memory>
#include <vector>
#include <unordered_map>
#include <unistd.h>

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
    bool _canceled;        // true--取消定时任务，false--不取消
    TaskFunc _task_cb;    // 定时器要执行的定时任务
    ReleaseFunc _release; // 用于删除TimerWheel中保存的定时任务对象信息(删除unordered_map中的元素)
};

// 时间轮类
class TimerWheel
{
public:
    TimerWheel()
        :_tick(0)
        , _capacity(60)
        , _wheel(_capacity)
    {}

    // 添加定时任务
    void TimerAdd(uint64_t id, uint32_t delay, const TaskFunc& cb)
    {
        SharedTask st(new TimerTask(id, delay, cb));
        st->SetRelease(std::bind(&TimerWheel::RemoveTimer, this, id));
        _timers[id] = WeakTask(st);

        // 将定时任务添加到时间轮中
        int pos = (_tick + delay) % _capacity;
        _wheel[pos].push_back(st);
    }

    // 刷新/延迟定时任务
    void TimerRefresh(uint64_t id)
    {
        // 通过定时器对象的weak_ptr构造一个shared_ptr，再添加到时间轮中
        auto it = _timers.find(id);
        if (it == _timers.end()) return;

        SharedTask st = it->second.lock();
        int delay = st->DelayTime();
        int pos = (_tick + delay) % _capacity;
        _wheel[pos].push_back(st);
    }

    void TimerCancel(uint64_t id)
    {
        auto it = _timers.find(id);
        if (it == _timers.end()) return;

        SharedTask st = it->second.lock();
        if (st) st->Cancel();
    }

    // 时间轮定时任务执行函数(该函数每秒执行一次)
    void RunTimerTask()
    {
        _tick = (_tick + 1) % _capacity;
        _wheel[_tick].clear(); // 清空指针位置上的数组，将所有管理定时任务对象的shared_ptr给释放掉
    }

private:
    void RemoveTimer(uint64_t id)
    {
        auto it = _timers.find(id);
        if (it != _timers.end())
        {
            _timers.erase(it);
        }
    }

private:
    using SharedTask = std::shared_ptr<TimerTask>;
    using WeakTask = std::weak_ptr<TimerTask>;

    int _tick;                                      // 当前的秒针(走到哪里释放哪里，释放哪里就相当于执行哪里的定时任务)
    int _capacity;                                  // 轮的最大数量（最大延迟时间）
    std::vector<std::vector<SharedTask>> _wheel;    // 时间轮
    std::unordered_map<uint64_t, WeakTask> _timers; // 将定时任务id和管理定时任务的weak_ptr绑定
};

class Test
{
public:
    Test() { std::cout << "构造" << std::endl; }
    ~Test() { std::cout << "析构" << std::endl; }
};

void DeleteTest(Test* t)
{
    delete t;
}

int main()
{
    TimerWheel tw;
    Test* t = new Test();

    tw.TimerAdd(888, 5, std::bind(DeleteTest, t));

    for (int i = 0; i < 5; ++i)
    {
        sleep(1);
        tw.TimerRefresh(888);
        tw.RunTimerTask();
        std::cout << "刷新了一下定时任务，重新需要5s后才能执行定时任务" << std::endl;
    }

    tw.TimerCancel(888); // 将定时任务取消

    while (true)
    {
        sleep(1);
        std::cout << "--------------------" << std::endl;
        tw.RunTimerTask();
    }

    return 0;
}