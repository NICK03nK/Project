#include "util.hpp"
#include "db.hpp"
#include "online.hpp"
#include "room.hpp"

void json_test()
{
    Json::Value student;
    student["name"] = "nK";
    student["age"] = 21;
    student["score"].append(98);
    student["score"].append(100);
    student["score"].append(80);

    std::string str;
    json_util::serialize(student, str);
    
    DLOG("序列化结果:");
    DLOG("%s", str.c_str());

    Json::Value studentTmp;
    json_util::unserialize(str, studentTmp);

    DLOG("反序列化结果:");
    DLOG("name: %s", studentTmp["name"].asString().c_str());
    DLOG("age: %d", studentTmp["age"].asInt());
    for (int i = 0; i < studentTmp["score"].size(); ++i)
    {
        DLOG("score: %d", studentTmp["score"][i].asInt());
    }
}

void string_test()
{
    std::string src = "123+222+++11+";
    std::vector<std::string> res;
    string_util::split(src, "+", res);

    for (const auto& e : res)
    {
        DLOG("%s", e.c_str());
    }
}

void file_test()
{
    std::string filename = "./Makefile";
    std::string body;
    file_util::read(filename, body);

    std::cout << body << std::endl;
}

void db_test()
{
    user_table utb("127.0.0.1", "root", "", "online_gobang", 3306);
    // ------------------------------测试注册用户
    // Json::Value user1;
    // user1["username"] = "凤梨兄";
    // user1["password"] = "123123";

    // utb.signup(user1);

    // ------------------------------测试登录
    // Json::Value loginUser;
    // loginUser["username"] = "nKk";
    // loginUser["password"] = "123123123";
    // if (utb.login(loginUser)) std::cout << "true" << std::endl;

    // std::cout << loginUser["id"].asInt() << std::endl;
    // std::cout << loginUser["username"].asString() << std::endl;
    // std::cout << loginUser["password"].asString() << std::endl;
    // std::cout << loginUser["score"].asInt() << std::endl;
    // std::cout << loginUser["total_count"].asInt() << std::endl;
    // std::cout << loginUser["win_count"].asInt() << td::endl;
    
    // ------------------------------测试select by username
    // Json::Value userInfo;
    // // if (utb.select_by_username("凤梨兄", userInfo))
    // if (utb.select_by_username("凤梨", userInfo))
    // {
    //     std::cout << userInfo["id"].asInt() << std::endl;
    //     std::cout << userInfo["username"].asString() << std::endl;
    //     std::cout << userInfo["score"].asInt() << std::endl;
    //     std::cout << userInfo["total_count"].asInt() << std::endl;
    //     std::cout << userInfo["win_count"].asInt() << std::endl;
    // }

    // ------------------------------测试select by id
    // Json::Value userInfo;
    // // if (utb.select_by_id(1, userInfo))
    // if (utb.select_by_id(3, userInfo))
    // {
    //     std::cout << userInfo["id"].asInt() << std::endl;
    //     std::cout << userInfo["username"].asString() << std::endl;
    //     std::cout << userInfo["score"].asInt() << std::endl;
    //     std::cout << userInfo["total_count"].asInt() << std::endl;
    //     std::cout << userInfo["win_count"].asInt() << std::endl;
    // }

    // ------------------------------测试victory&defeat
    // utb.victory(3);
    utb.defeat(1);
}

void online_test()
{
    online_manager om;

    websocketsvr_t::connection_ptr conn1;
    uint64_t id1 = 20;
    om.enter_game_hall(id1, conn1);
    if (om.is_in_game_hall(id1)) DLOG("in game hall");
    else DLOG("not in game hall");
    om.exit_game_hall(id1);
    if (om.is_in_game_hall(id1)) DLOG("in game hall");
    else DLOG("not in game hall");

    websocketsvr_t::connection_ptr conn2;
    uint64_t id2 = 22;
    om.enter_game_room(id2, conn2);
    if (om.is_in_game_room(id2)) DLOG("in game room");
    else DLOG("not in game room");
    om.exit_game_room(id2);
    if (om.is_in_game_room(id2)) DLOG("in game room");
    else DLOG("not in game room");
}

void room_test()
{
    user_table utb("127.0.0.1", "root", "", "online_gobang", 3306);
    online_manager om;

    room r(20, &utb, &om);

    Json::Value req;
    req["optype"] = "chat";
    req["room_id"] = 222;
    req["uid"] = 1;
    req["message"] = "我不是基佬！";

    Json::Value resp = r.handle_chat(req);
    std::string resp_str;
    json_util::serialize(resp, resp_str);
    std::cout << resp_str << std::endl;

    // room_manager rm(&utb, &om);
}

int main()
{
    // json_test();
    // string_test();
    // file_test();
    // db_test();
    // online_test();
    room_test();
    
    return 0;
}