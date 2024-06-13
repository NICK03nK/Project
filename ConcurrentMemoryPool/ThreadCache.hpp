#pragma once

#include <iostream>
#include <cassert>

#include "FreeList.hpp"
#include "SizeClass.hpp"

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
        return nullptr;
    }

private:
    FreeList _freeListBucket[N_FREELISTS]; // 自由链表桶
};

static _declspec(thread) ThreadCache* pTLSThreadCache = nullptr; // 将pTLSThreadCache声明为线程局部存储的指针，指向ThreadCache对象