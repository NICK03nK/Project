#pragma once

#include <cassert>

// 访问小块内存中的头4/8个字节，即下一个内存块的地址
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

    // 向自由链表中插入多个内存块
    void PushRange(void* start, void* end)
    {
        NextObj(end) = _freeList;
        _freeList = start;
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

    size_t& MaxSize() { return _maxSize; }

private:
    void* _freeList = nullptr;
    size_t _maxSize = 1;
};