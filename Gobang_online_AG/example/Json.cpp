#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <jsoncpp/json/json.h>

// 序列化
std::string serialize(const Json::Value& root)
{
    // 1. 实例化一个StreamWriterBuilder工厂类对象
    Json::StreamWriterBuilder swb;

    // 2. 通过StreamWriterBuilder工厂类对象实例化一个StreamWriter对象
    Json::StreamWriter* sw = swb.newStreamWriter();

    // 3. 使用StreamWriter对象，对Json::Value对象中存储的数据进行序列化
    std::stringstream ss;
    sw->write(root, &ss);

    delete sw; // sw是new出来的记得释放

    return ss.str();
}

// 反序列化
Json::Value unserialize(const std::string& str)
{
    // 1. 实例化一个CharReaderBuilder工厂类对象
    Json::CharReaderBuilder crb;

    // 2. 通过CharReaderBuilder工厂类对象实例化一个CharReader对象
    Json::CharReader* cr = crb.newCharReader();

    // 3. 创建一个Json::Value对象存储解析后的数据
    Json::Value root;

    // 4. 使用CharReader对象，对str字符串进行Json格式的反序列化
    std::string err;
    cr->parse(str.c_str(), str.c_str() + str.size(), &root, &err);

    delete cr;

    return root;
}

// 使用JosnCpp库进行多个数据对象的序列化与反序列化
int main()
{
    // 将需要进行序列化的数据存储到Json::Value对象中
    Json::Value student;
    student["name"] = "nK";
    student["age"] = 21;
    student["score"].append(98);
    student["score"].append(100);
    student["score"].append(80);

    std::string str = serialize(student);
    std::cout << "序列化结果：\n" << str << std::endl;

    Json::Value studentTmp = unserialize(str);

    // 输出Json格式的数据
    std::cout << "反序列化结果：" << std::endl;
    std::cout << "name: " << studentTmp["name"].asString() << std::endl;
    std::cout << "age: " << studentTmp["age"].asInt() << std::endl;
    for (int i = 0; i < studentTmp["score"].size(); ++i)
    {
        std::cout << "score" << studentTmp["score"][i].asFloat() << std::endl;
    }    

    return 0;
}