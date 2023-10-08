#include "logger.hpp"
#include "util.hpp"
#include "db.hpp"
#include "online.hpp"
#include "room.hpp"
#include "session.hpp"
#include "matcher.hpp"

#define HOST "127.0.0.1"
#define USER "root"
#define PASSWORD "N18124665842@"
#define DB "gobang"
#define PORT 3306

void mysql_util_test()
{
    MYSQL* mysql = mysql_util::mysql_create(HOST, USER, PASSWORD, DB, PORT);

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

void db_test()
{
    user_table ut(HOST, USER, PASSWORD, DB, PORT);

    Json::Value user;
    user["username"] = "zhangsan";
    // user["password"] = "123123";
    // user["password"] = "1231244";

    ut.signup(user);
    
    // bool ret = ut.signin(user);
    // if (ret == false)
    // {
    //     DLOG("SIGN IN FAILED");
    //     return;
    // }

    // bool ret = ut.select_by_name("zhangsan", user);
    // std::string body;
    // json_util::serialize(user, body);
    // DLOG("%s", body.c_str());

    // bool ret = ut.select_by_id(2, user);
    // std::string body;
    // json_util::serialize(user, body);
    // DLOG("%s", body.c_str());

    // ut.victory(1);

    // ut.defeat(2);
}

void online_test()
{
    online_manager om;
    wsserver_t::connection_ptr conn;
    uint64_t userId = 123;

    // om.enter_game_hall(userId, conn);
    // if (om.is_in_game_hall(userId))
    // {
    //     DLOG("IN GAME HALL");
    // }
    // else
    // {
    //     DLOG("NOT IN GAME HALL");
    // }

    // om.exit_game_hall(userId);
    // if (om.is_in_game_hall(userId))
    // {
    //     DLOG("IN GAME HALL");
    // }
    // else
    // {
    //     DLOG("NOT IN GAME HALL");
    // }

    om.enter_game_room(userId, conn);
    if (om.is_in_game_room(userId))
    {
        DLOG("IN GAME ROOM");
    }
    else
    {
        DLOG("NOT IN GAME ROOM");
    }

    om.exit_game_room(userId);
    if (om.is_in_game_room(userId))
    {
        DLOG("IN GAME ROOM");
    }
    else
    {
        DLOG("NOT IN GAME ROOM");
    }
}

void room_test()
{
    user_table ut(HOST, USER, PASSWORD, DB, PORT);
    online_manager om;
    // room r(3, &ut, &om);
    
    room_manager rm(&ut, &om);
    room_ptr rp = rm.create_room(10, 20);
}

void match_test()
{
    user_table ut(HOST, USER, PASSWORD, DB, PORT);
    online_manager om;
    room_manager rm(&ut, &om);
    matcher mc(&rm, &ut, &om);
}

int main()
{
    // mysql_util_test();
    // json_util_test();
    // string_util_test();
    // file_util_test();
    // db_test();
    // online_test();
    // room_test();
    match_test();

    return 0;
}