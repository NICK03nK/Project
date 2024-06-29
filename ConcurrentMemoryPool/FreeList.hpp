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
        ++_size;
    }

    // 向自由链表中插入多个内存块
    void PushRange(void* start, void* end, size_t n)
    {
        NextObj(end) = _freeList;
        _freeList = start;
        _size += n;
    }

    void* Pop()
    {
        assert(_freeList);

        // 头删
        void* obj = _freeList;
        _freeList = NextObj(obj);
        --_size;

        return obj;
    }

    void PopRange(void*& start, void*& end, size_t n)
    {
        assert(n >= _size);
        
        start = _freeList;
        end = start;

        for (int i = 0; i < n - 1; ++i)
        {
            end = NextObj(end);
        }

        _freeList = NextObj(end);
        NextObj(end) = nullptr;
        _size -= n;
    }

    bool Empty() { return _freeList == nullptr; }

    size_t Size() { return _size; }

    size_t& MaxSize() { return _maxSize; }

private:
	void* _freeList = nullptr; // 自由链表的头指针
	size_t _maxSize = 1;       // 结合慢增长来使用
    size_t _size = 0;          // 记录自由链表中小内存块的个数
};