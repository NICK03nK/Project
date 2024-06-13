#pragma once

#include <cassert>

// 访问小块内存中的头4/8个字节
static void*& NextObj(void* obj) { return *(void**)obj; }

// 管理切割好的内存块的自由链表
class FreeList
{
public:
    void Push(void* obj)
    {
        assert(obj);

        // 头插
        NextObj(obj) = _freeList;
        _freeList = obj;
    }

    void* Pop()
    {
        assert(_freeList);

        // 头删
        void* obj = _freeList;
        _freeList = NextObj(obj);

        return obj;
    }

    bool Empty() { return _freeList == nullptr; }

private:
    void* _freeList = nullptr;
};