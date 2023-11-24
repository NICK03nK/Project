#include <iostream>
#include <string>
#include <regex>

int main()
{
    // HTTP请求行格式：GET /home/login?user=nk&pass=123123 HTTP/1.1\r\n
    std::string str = "GET /home/login?user=nk&pass=123123 HTTP/1.1\r\n";
    std::smatch matches;

    // HTTP请求的方法匹配
    // GET POST PUT HEAD DELETE OPTIONS TRACE CONNECT LINK UNLINE
    // std::regex e("(GET|POST|PUT|HEAD|DELETE|OPTIONS|TRACE|CONNECT|LINK|UNLINE) .*");

    // HTTP请求的资源路径匹配
    // std::regex e("(GET|POST|PUT|HEAD|DELETE|OPTIONS|TRACE|CONNECT|LINK|UNLINE) ([^?]*).*");

    // HTTP请求的查询字符串匹配
    // std::regex e("(GET|POST|PUT|HEAD|DELETE|OPTIONS|TRACE|CONNECT|LINK|UNLINE) ([^?]*)\\?(.*) .*");

    // HTTP请求的协议版本匹配
    std::regex e("(GET|POST|PUT|HEAD|DELETE|OPTIONS|TRACE|CONNECT|LINK|UNLINE) ([^?]*)(?:\\?(.*))? (HTTP/1\\.[01])(?:\n|\r\n)?");

    bool ret = std::regex_match(str, matches, e);
    if (ret == false) return -1;

    for (const auto& e : matches)
    {
        std::cout << e << std::endl;
    }

    return 0;
}