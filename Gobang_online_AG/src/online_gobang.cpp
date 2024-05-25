#include "server.hpp"

int main()
{
    gobang_server server("127.0.0.1", "root", "", "online_gobang", 3306);
    server.start(8080);

    return 0;
}