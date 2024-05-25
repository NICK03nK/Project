#include <unistd.h>

#include "server.hpp"

int main()
{
    // 将游戏服务器设置为守护进程
    if (daemon(1, 0) == -1) {
        perror("daemon");
        exit(EXIT_FAILURE);
    }

    gobang_server server("127.0.0.1", "root", "", "online_gobang", 3306);
    server.start(8080);

    return 0;
}