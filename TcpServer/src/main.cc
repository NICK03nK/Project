#include "server.hpp"

int main()
{
    Buffer buf;
    for (int i = 0; i < 300; ++i)
    {
        std::string str = "hello!!" + std::to_string(i) + '\n';
        buf.WriteStringAndPush(str);
    }

    while (buf.ReadableSize())
    {
        std::string line = buf.GetLineAndPop();
        std::cout << line << std::endl;
    }

    /*
    Buffer buf;

    std::string str = "hello!!";
    buf.WriteStringAndPush(str);

    Buffer buf1;
    buf1.WriteBufferAndPush(buf);

    std::string tmp = buf1.ReadAsStringAndPop(buf1.ReadableSize());

    std::cout << tmp << std::endl;
    std::cout << buf.ReadableSize() << std::endl;
    std::cout << buf1.ReadableSize() << std::endl;
    */

    return 0;
}