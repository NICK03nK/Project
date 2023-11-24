#include <iostream>
#include <typeinfo>
#include <cassert>
#include <string>

class Any
{
public:
    Any()
        :_content(nullptr)
    {}

    template<class T>
    Any(const T& val)
        :_content(new placeholder<T>(val))
    {}

    Any(const Any& other)
        :_content(other._content ? other._content->clone() : nullptr)
    {}

    ~Any() { delete _content; }

    Any& swap(Any& other)
    {
        std::swap(_content, other._content);
        return *this;
    }

    // 获取Any对象中保存的数据的地址
    template<class T>
    T* get()
    {
        assert(typeid(T) == _content->type()); // 想要获取的数据类型必须和保存的数据类型一致
        return &((placeholder<T>*)_content)->_val;
    }

    template<class T>
    Any& operator=(const T& val)
    {
        Any(val).swap(*this);
        return *this;
    }

    Any& operator=(const Any& other)
    {
        Any(other).swap(*this);
        return *this;
    }

private:
    class holder
    {
    public:
        virtual ~holder() {}

        virtual const std::type_info& type() = 0;

        virtual holder* clone() = 0;
    };

    template <class T>
    class placeholder : public holder
    {
    public:
        placeholder(const T& val)
            :_val(val)
        {}

        // 获取子类对象保存的数据类型
        virtual const std::type_info& type() { return typeid(T); }

        // 根据当前对象克隆出一个新的子类对象
        virtual holder* clone() { return new placeholder(_val); }

    public:
        T _val;
    };

    holder* _content;
};

class Test
{
public:
    Test() { std::cout << "构造" << std::endl; }
    Test(const Test& t) { std::cout << "拷贝" << std::endl; }
    ~Test() { std::cout << "析构" << std::endl; }
};

int main()
{
    {
        Any a;
        Test t;
        a = t;
    }

    // Any a;
    // a = 10;
    // int* pa = a.get<int>();
    // std::cout << *pa << std::endl;

    // a = std::string("hello");
    // std::string* ps = a.get<std::string>();
    // std::cout << *ps << std::endl;

    return 0;
}