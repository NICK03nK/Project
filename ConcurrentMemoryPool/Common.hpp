#pragma once

#include <iostream>
#include <cassert>
#include <mutex>
#include <unordered_map>
#include <Windows.h>
#include <cstdlib>
#include <new>
#include <algorithm>

static const size_t MAX_BYTES = 256 * 1024; // thread cache中最大可申请的内存为256KB
static const size_t N_FREELISTS = 208;      // 哈希桶的自由链表个数
static const size_t N_PAGES = 129;          // page cache中页数的上限，[0, 128]
static const size_t PAGE_SHIFT = 13;        // 一个页的大小为2^13byte，即8KB

#ifdef _WIN64
typedef unsigned long long PAGE_ID;
#elif _WIN32
typedef size_t PAGE_ID;
#endif

// 直接去堆上按页申请空间
inline static void* SystemAlloc(size_t nPages)
{
	void* ptr = VirtualAlloc(0, nPages << 13, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (ptr == nullptr) throw std::bad_alloc();

	return ptr;
}

// 释放在堆上申请的空间
inline static void SystemFree(void* ptr)
{
	VirtualFree(ptr, 0, MEM_RELEASE);
}

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

class SizeClass
{
public:
    // 整体控制在最多10%左右的内碎片浪费
    // [1, 128]                 8byte对齐         freelist[0,16)
    // [128+1, 1024]            16byte对齐        freelist[16,72)
    // [1024+1, 8*1024]         128byte对齐       freelist[72,128)
    // [8*1024+1, 64*1024]      1024byte对齐      freelist[128,184)
    // [64*1024+1, 256*1024]    8*1024byte对齐    freelist[184,208)

    // 计算按bytes大小申请内存实际申请到的内存大小
    static inline size_t RoundUp(size_t bytes)
    {
        if (bytes <= 128)
        {
            return _RoundUp(bytes, 8);
        }
        else if (bytes <= 1024)
        {
            return _RoundUp(bytes, 16);
        }
        else if (bytes <= 8 * 1024)
        {
            return _RoundUp(bytes, 128);
        }
        else if (bytes <= 64 * 1024)
        {
            return _RoundUp(bytes, 1024);
        }
        else if (bytes <= 256 * 1024)
        {
            return _RoundUp(bytes, 8 * 1024);
        }
        else
        {
            return _RoundUp(bytes, 1 << PAGE_SHIFT);
        }

        return -1;
    }

    // 计算映射到哪个自由链表桶
    static inline size_t Index(size_t bytes)
    {
        assert(bytes <= MAX_BYTES);

        static int group_array[4] = { 16, 56, 56, 56 }; // 每个区间有多少个自由链表桶

        if (bytes <= 128)
        {
            return _Index(bytes, 3);
        }
        else if (bytes <= 1024)
        {
            return _Index(bytes - 128, 4) + group_array[0];
        }
        else if (bytes <= 8 * 1024)
        {
            return _Index(bytes - 1024, 7) + group_array[0] + group_array[1];
        }
        else if (bytes <= 64 * 1024)
        {
            return _Index(bytes - 8 * 1024, 10) + group_array[0] + group_array[1] + group_array[2];
        }
        else if (bytes <= 256 * 1024)
        {
            return _Index(bytes - 64 * 1024, 13) + group_array[0] + group_array[1] + group_array[2] + group_array[3];
        }
        else assert(false);

        return -1;
    }

    // thread cache一次从central cache中获取多少个内存对象
    static inline size_t NumMoveSize(size_t size)
    {
        assert(size > 0);

        // 一次性批量移动内存对象的取值为：[2, 512]
        // 小内存对象一次性移动的数量上限高
        // 大内存对象一次性移动的数量上限低
        int num = MAX_BYTES / size;
        if (num < 2) num = 2;
        if (num > 512) num = 512;

        return num;
    }

    // 计算一次向操作系统获取几个页
    static inline size_t NumMovePage(size_t size)
    {
        size_t batchNum = NumMoveSize(size);
        size_t bytes = batchNum * size; // 要申请的内存空间的总字节数

        // 计算向操作系统申请的页数
        size_t nPages = bytes >> PAGE_SHIFT;
        if (nPages == 0) nPages = 1; // 要申请的内存空间不足一个页的大小（不足8KB），则按一个页申请

        return nPages;
    }

private:
    // RoundUp()的辅助函数
    static inline size_t _RoundUp(size_t bytes, size_t align)
    {
        return (((bytes)+align - 1) & ~(align - 1));
    }

    // Index()的辅助函数
    static inline size_t _Index(size_t bytes, size_t align_shift)
    {
        return ((bytes + (1 << align_shift) - 1) >> align_shift) - 1;
    }
};

// 管理多个连续页大块内存跨度的结构
struct Span
{
    PAGE_ID _pageId = 0;       // 页号
    size_t _nPages = 0;		   // 页的数量
    Span* _prev = nullptr;     // 指向前一个Span对象
    Span* _next = nullptr;     // 指向后一个Span
    size_t _useCount = 0;	   // 切割的小块内存，分配给thread cache的计数
    void* _freeList = nullptr; // 管理切割的小块内存的自由链表
    bool _isUse = false;       // 标记该span是否被使用
};

// 带头双向循环链表
class SpanList
{
public:
    SpanList()
    {
        _head = new Span();
        _head->_prev = _head;
        _head->_next = _head;
    }

    Span* Begin() { return _head->_next; }

    Span* End() { return _head; }

    bool Empty() { return _head->_next == _head; }

    void PushFront(Span* newSpan)
    {
        Insert(Begin(), newSpan);
    }

    Span* PopFront()
    {
        Span* front = _head->_next;
        Erase(front);
        return front;
    }

    void Insert(Span* pos, Span* newSpan)
    {
        assert(pos);
        assert(newSpan);

        Span* prev = pos->_prev;
        prev->_next = newSpan;
        newSpan->_prev = prev;
        newSpan->_next = pos;
        pos->_prev = newSpan;
    }

    void Erase(Span* pos)
    {
        assert(pos);
        assert(pos != _head);

        Span* prev = pos->_prev;
        Span* next = pos->_next;

        prev->_next = next;
        next->_prev = prev;
    }

    std::mutex& GetMutex() { return _mtx; }

private:
    Span* _head;     // 头结点
    std::mutex _mtx; // 桶锁，属于当前这个自由链表桶的锁
};