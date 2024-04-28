#include "util.hpp"

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

int main()
{
    // json_test();
    string_test();
    
    return 0;
}