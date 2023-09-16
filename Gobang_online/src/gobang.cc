#include "logger.hpp"
#include "util.hpp"

#define HOST "127.0.0.1"
#define USER "root"
#define PASSWD "N18124665842@"
#define DB "gobang"
#define PORT 3306

void mysql_util_test()
{
    MYSQL* mysql = mysql_util::mysql_create(HOST, USER, PASSWD, DB, PORT);

    const char* sql = "insert stu values(null, 'lisi', 21, 130, 109, 97);";
    bool ret = mysql_util::mysql_exec(mysql, sql);
    if (ret == false)
    {
        return;
    }

    mysql_util::mysql_destroy(mysql);
}

void json_util_test()
{
    Json::Value root;
    std::string body;

    root["name"] = "zhangsan";
    root["age"] = 20;
    root["score"].append(98);
    root["score"].append(94.8);
    root["score"].append(100);

    json_util::serialize(root, body);
    DLOG("%s", body.c_str());

    Json::Value val;
    json_util::unserialize(body, val);

    std::cout << "name: " << val["name"].asString() << std::endl;
    std::cout << "age: " << val["age"].asInt() << std::endl;
    for (int i = 0; i < val["score"].size(); ++i)
    {
        std::cout << "socre:" << val["score"][i].asFloat() << std::endl;
    }
}

void string_util_test()
{
    std::string src = "123.234....345";
    std::string sep = ".";
    std::vector<std::string> res;

    int n = string_util::split(src, sep, res);
    for (const auto& str : res)
    {
        DLOG("%s", str.c_str());
    }
    // for (int i = 0; i < n; ++i)
    // {
    //     DLOG("%s", res[i].c_str());
    // }
}

void file_util_test()
{
    // std::string filename = "logger.hpp";
    std::string filename;
    std::string body;

    std::cin >> filename;
    file_util::read(filename, body);

    DLOG("%s", body.c_str());
}

int main()
{
    // mysql_util_test();
    // json_util_test();
    // string_util_test();
    file_util_test();

    return 0;
}