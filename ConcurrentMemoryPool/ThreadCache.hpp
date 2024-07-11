#pragma once

#include "Common.hpp"
#include "CentralCache.hpp"

class ThreadCache
{
public:
    // 从thread cache中申请内存
    void* Allocate(size_t size)
    {
        assert(size <= MAX_BYTES);

        size_t alignSize = SizeClass::RoundUp(size); // 实际申请到的内存大小
        size_t index = SizeClass::Index(size);       // 映射到哈希桶的下标

        // 优先使用自由链表中管理的内存块
        if (!_freeListBucket[index].Empty())
        {
            return _freeListBucket[index].Pop();
        }
        else
        {
            return FetchFromCentralCache(index, alignSize); // 自由链表中没有内存块，向central cache申请内存
        }
    }

    // 将内存块归还给thread cache
    void Deallocate(void* ptr, size_t size)
    {
        assert(ptr);
        assert(size <= MAX_BYTES);

        // 计算对应的自由链表桶的下标，将归还的内存块头插到该下标的自由链表中
        size_t index = SizeClass::Index(size);
        _freeListBucket[index].Push(ptr);

        // 当链表长度大于等于一次批量申请的内存时就将freeList中的一段小内存块归还给central cache
        if (_freeListBucket[index].Size() >= _freeListBucket[index].MaxSize())
        {
            ListTooLong(_freeListBucket[index], size);
        }
    }

    // 从central cache中申请内存（获取thread cache对象）
    void* FetchFromCentralCache(size_t index, size_t size)
    {
        // 慢开始反馈调节算法，批量获取内存块
        size_t batchNum = min(_freeListBucket[index].MaxSize(), SizeClass::NumMoveSize(size));
        if (_freeListBucket[index].MaxSize() == batchNum)
        {
            _freeListBucket[index].MaxSize() += 1; // 慢增长
        }

        // 向central cache申请batchNum个内存块
        void* start = nullptr;
        void* end = nullptr;
        size_t actualNum = CentralCache::GetInstance()->FetchRangeObj(start, end, batchNum, size);
        assert(actualNum >= 1);
        
        if (actualNum == 1)
        {
            // 实际申请到的内存块数量只有1个，则start和end相等
            assert(start == end);
        }
        else
        {
            // 将申请到的多个内存块插入自由链表桶下挂的自由链表中
            _freeListBucket[index].PushRange(NextObj(start), end, actualNum - 1);
        }

        return start;
    }

    // 处理thread cache中过长的freeList
    void ListTooLong(FreeList& freeList, size_t size)
    {
        void* start = nullptr;
        void* end = nullptr;
        freeList.PopRange(start, end, freeList.MaxSize()); // 从freeList中取出批量个小内存块

        CentralCache::GetInstance()->ReleaseListToSpans(start, size); // 将批量的小内存块归还给central cache
    }

private:
    FreeList _freeListBucket[N_FREELISTS]; // 自由链表桶
};

static _declspec(thread) ThreadCache* pTLSThreadCache = nullptr; // 将pTLSThreadCache声明为线程局部存储的指针，指向ThreadCache对象