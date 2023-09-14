#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <jsoncpp/json/json.h>

//使用jsoncpp库进行对多个数据对象序列化
std::string serialize()
{
    // 1.将需要序列化的数据存储到Json::Value对象中
    Json::Value stu;
    stu["name"] = "zhangsan";
    stu["age"] = 18;
    stu["score"].append(107);
    stu["score"].append(120);
    stu["score"].append(132);

    // 2.实例化一个StreamWriterBuilder工厂类对象
    Json::StreamWriterBuilder swb;

    // 3.通过StreamWriterBuilder工厂类对象生产一个StreamWriter对象
    Json::StreamWriter* sw = swb.newStreamWriter();

    // 4.使用StreamWriter对象，对Json::Value中的数据进行序列化
    std::stringstream ss;
    sw->write(stu, &ss);
    std::cout << ss.str() << std::endl;

    delete sw;

    return ss.str();
}

void unserialize(const std::string& str)
{
    // 1.实例化一个CharReaderBuilder工厂类对象
    Json::CharReaderBuilder crb;

    // 2.通过CharReaderBuilder工厂类对象生产一个CharReader对象
    Json::CharReader* cr = crb.newCharReader();

    // 3.定义一个Josn::Value对象存储解析后的数据
    Json::Value root;

    // 4.使用CharReader对象，对json格式的字符串进行反序列化
    std::string errs;
    bool ret = cr->parse(str.c_str(), str.c_str() + str.size(), &root, &errs);
    if (ret == false)
    {
        std::cout << "json unserialize failed: " << errs << std::endl;
        return;
    }

    // 5.逐个元素访问Json::Value对象中的数据
    std::cout << "name: " << root["name"].asString() << std::endl;
    std::cout << "age: " << root["age"].asInt() << std::endl;
    
    for (int i = 0; i < root["score"].size(); ++i)
    {
        std::cout << "socre:" << root["score"][i].asFloat() << std::endl;
    }

    delete cr;
}

int main()
{
    std::string str = serialize();
    unserialize(str);

    return 0;
}