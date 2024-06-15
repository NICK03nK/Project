#pragma once

#include <iostream>
#include <cassert>
#include <algorithm>

#include "FreeList.hpp"
#include "SizeClass.hpp"
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
    }

    // 从central cache中申请内存（获取thread cache对象）
    void* FetchFromCentralCache(size_t index, size_t size)
    {
        // 慢开始反馈调节算法，批量获取内存块
        size_t batchNum = std::min(_freeListBucket[index].MaxSize(), SizeClass::NumMoveSize(size));
        if (_freeListBucket[index].MaxSize() == batchNum)
        {
            _freeListBucket[index].MaxSize() += 1; // 慢增长
        }

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
            _freeListBucket[index].PushRange(NextObj(start), end);
        }

        return start;
    }

private:
    FreeList _freeListBucket[N_FREELISTS]; // 自由链表桶
};

static _declspec(thread) ThreadCache* pTLSThreadCache = nullptr; // 将pTLSThreadCache声明为线程局部存储的指针，指向ThreadCache对象