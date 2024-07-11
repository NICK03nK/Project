#include <iostream>
#include <vector>
#include <thread>

#include "ObjectPool.hpp"
#include "ConcurrentAlloc.hpp"

struct TreeNode
{
    int _val;
    TreeNode* _left;
    TreeNode* _right;

    TreeNode()
        :_val(0)
        , _left(nullptr)
        , _right(nullptr)
    {}
};

void TestObjectPool()
{
    // 申请释放的轮次
    const size_t Rounds = 3;

    // 每轮申请释放多少次
    const size_t N = 10000;
    size_t begin1 = clock();
    std::vector<TreeNode* > v1;
    v1.reserve(N);

    for (size_t j = 0; j < Rounds; ++j)
    {
        for (int i = 0; i < N; ++i) v1.push_back(new TreeNode);
        for (int i = 0; i < N; ++i) delete v1[i];
        v1.clear();
    }

    size_t end1 = clock();
    ObjectPool<TreeNode> TNPool;
    size_t begin2 = clock();
    std::vector<TreeNode*> v2;
    v2.reserve(N);

    for (size_t j = 0; j < Rounds; ++j)
    {
        for (int i = 0; i < N; ++i) v2.push_back(TNPool.New());
        for (int i = 0; i < N; ++i) TNPool.Delete(v2[i]);
        v2.clear();
    }

    size_t end2 = clock();
    std::cout << "new cost time:" << end1 - begin1 << std::endl;
    std::cout << "object pool cost time:" << end2 - begin2 << std::endl;
}

void Alloc1()
{
    for (int i = 0; i < 5; ++i)
    {
        void* ptr = ConcurrentAlloc(5);
    }
}

void Alloc2()
{
    for (int i = 0; i < 5; ++i)
    {
        void* ptr = ConcurrentAlloc(7);
    }
}

void TLSTest()
{
    std::thread t1(Alloc1);
    std::thread t2(Alloc2);

    t1.join();
    t2.join();
}

void TestConcurrentAlloc1()
{
    void* p1 = ConcurrentAlloc(6);
    void* p2 = ConcurrentAlloc(8);
    void* p3 = ConcurrentAlloc(1);
    void* p4 = ConcurrentAlloc(7);
    void* p5 = ConcurrentAlloc(8);
    void* p6 = ConcurrentAlloc(2);
    void* p7 = ConcurrentAlloc(1);

    std::cout << p1 << std::endl;
    std::cout << p2 << std::endl;
    std::cout << p3 << std::endl;
    std::cout << p4 << std::endl;
    std::cout << p5 << std::endl;
    std::cout << p6 << std::endl;
    std::cout << p7 << std::endl;
}

void TestConcurrentAlloc2()
{
    for (size_t i = 0; i < 1024; ++i)
    {
        void* p1 = ConcurrentAlloc(6);
    }

    void* p2 = ConcurrentAlloc(8);
    std::cout << p2 << std::endl;
}

void TestConcurrentFree()
{
    void* p1 = ConcurrentAlloc(6);
    void* p2 = ConcurrentAlloc(8);
    void* p3 = ConcurrentAlloc(1);
    void* p4 = ConcurrentAlloc(7);
    void* p5 = ConcurrentAlloc(8);
    void* p6 = ConcurrentAlloc(2);
    void* p7 = ConcurrentAlloc(1);

    std::cout << p1 << std::endl;
    std::cout << p2 << std::endl;
    std::cout << p3 << std::endl;
    std::cout << p4 << std::endl;
    std::cout << p5 << std::endl;

    ConcurrentFree(p1);
    ConcurrentFree(p2);
    ConcurrentFree(p3);
    ConcurrentFree(p4);
    ConcurrentFree(p5);
    ConcurrentFree(p6);
    ConcurrentFree(p7);
}

void MultiThreadAlloc1()
{
    std::vector<void*> vt;
    for (int i = 0; i < 7; ++i)
    {
        void* ptr = ConcurrentAlloc(6);
        vt.push_back(ptr);
    }

    for (auto e : vt)
    {
        ConcurrentFree(e);
    }
}

void MultiThreadAlloc2()
{
    std::vector<void*> vt;
    for (int i = 0; i < 7; ++i)
    {
        void* ptr = ConcurrentAlloc(7);
        vt.push_back(ptr);
    }

    for (auto e : vt)
    {
        ConcurrentFree(e);
    }
}

void TestMultiThread()
{
    std::thread t1(MultiThreadAlloc1);
    std::thread t2(MultiThreadAlloc2);

    t1.join();
    t2.join();
}

void BigAlloc()
{
    void* p1 = ConcurrentAlloc(257 * 1024);
    ConcurrentFree(p1);

    void* p2 = ConcurrentAlloc(129 * 8 * 1024);
    ConcurrentFree(p2);
}

int main()
{
    //TestObjectPool();
    //TLSTest();

    //TestConcurrentAlloc1();
    //TestConcurrentAlloc2();

    TestConcurrentFree();

    //TestMultiThread();

    BigAlloc();

    return 0;
}