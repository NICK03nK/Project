#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/timerfd.h>

int main()
{
    int timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (timerfd < 0)
    {
        perror("timer_create error");
        return -1;
    }

    struct itimerspec itime;

    // 第一次超时时间为1s后
    itime.it_value.tv_sec = 1;
    itime.it_value.tv_nsec = 0;

    // 第一次超时后，每次超时的间隔时间为5s
    itime.it_interval.tv_sec = 5;
    itime.it_interval.tv_nsec = 0;

    timerfd_settime(timerfd, 0, &itime, nullptr);

    int i = 3;
    while (i--)
    {
        std::cout << "sleep" << std::endl;
        sleep(1);
    }

    uint64_t times;
    int ret = read(timerfd, &times, 8);
    if (ret < 0)
    {
        perror("read error");
        return -1;
    }

    std::cout << times << std::endl;

    close(timerfd);

    return 0;
}