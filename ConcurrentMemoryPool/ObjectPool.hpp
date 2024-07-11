#pragma once

#include "Common.hpp"

template<class T>
class ObjectPool
{
public:
    T* New()
    {
        T* obj = nullptr;

        if (_freeList) // 优先使用自由链表中的内存块
        {
            void* next = *(void**)_freeList;
            obj = (T*)_freeList;
            _freeList = next;
        }
        else
        {
            if (_remainingBytes < sizeof(T)) // 内存块剩余的字节小于T的大小
            {
                // 调用SystemAlloc申请大块内存
                _remainingBytes = 128 * 1024;
                _memory = (char*)SystemAlloc(_remainingBytes >> 13);
                if (_memory == nullptr) throw std::bad_alloc();
            }

            obj = (T*)_memory;
            size_t objSize = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);
            _memory += objSize;
            _remainingBytes -= objSize;
        }

        // 定位new，显示调用T的构造函数初始化
        new(obj)T;

        return obj;
    }

    void Delete(T* obj)
    {
        // 显示调用T的析构函数清理T对象中的资源
        obj->~T();

        // 将分割的内存块头插到自由链表中
        *(void**)obj = _freeList;
        _freeList = obj;
    }

private:
    char* _memory = nullptr;    // 指向内存块的指针
    size_t _remainingBytes = 0; // 内存块中剩下的字节数
    void* _freeList = nullptr;  // 管理归还的内存的自由链表
};