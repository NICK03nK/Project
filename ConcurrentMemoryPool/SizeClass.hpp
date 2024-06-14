#pragma once

#include <iostream>
#include <cassert>

static const size_t MAX_BYTES = 256 * 1024; // thread cache中最大可申请的内存为256KB
static const size_t N_FREELISTS = 208;      // 哈希桶的自由链表个数

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
        else assert(false);

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