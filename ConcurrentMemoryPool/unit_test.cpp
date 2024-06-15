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

int main()
{
    //TestObjectPool();

    TLSTest();

    return 0;
}