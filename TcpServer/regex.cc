#include <iostream>
#include <string>
#include <regex>

int main()
{
    std::string str = "/numbers/123";
    std::regex e("/numbers/(\\d+)"); // 匹配以/numbers/开头后跟一个或多个数字的字符，将匹配到的数字字符串提取出来
    std::smatch matches;

    bool ret = std::regex_match(str, matches, e);
    if (ret == false) return -1;

    for (const auto& e : matches)
    {
        std::cout << e << std::endl;
    }

    return 0;
}