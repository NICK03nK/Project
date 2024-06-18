#pragma once

#include <cassert>

#include "SpanList.hpp"
#include "SizeClass.hpp"
#include "PageCache.hpp"

// 单例模式--饿汉模式
class CentralCache
{
public:
	// 获取central cache的单例对象
	static CentralCache* GetInstance() { return &_singleInstance; }

	// 获取一个非空的span
	Span* GetOneSpan(SpanList& spanList, size_t size)
	{
		// 查看当前spanlist中是否存在_freeList不为空的span对象
		Span* it = spanList.Begin();
		while (it != spanList.End())
		{
			if (it->_freeList != nullptr)
			{
				return it;
			}
			else
			{
				it = it->_next;
			}
		}

		// 先将central cache中的spanlist的桶锁解锁，这样其他的线程在归还内存给这个spanlist的话就不会因为拿不到锁资源而阻塞
		spanList.GetMutex().unlock();

		// 代码执行到这里说明，当前spanlist中没有空闲的span了，需要向page cache申请span对象
		PageCache::GetInstance()->GetMutex().lock(); // page cache整体锁加锁
		Span* span = PageCache::GetInstance()->GetSpan(SizeClass::NumMovePage(size));
		PageCache::GetInstance()->GetMutex().unlock(); // page cache整体锁解锁

		// 计算span的大块内存的起始地址和大块内存的的字节数
		char* start = (char*)(span->_pageId << PAGE_SHIFT);
		size_t bytes = span->_nPages << PAGE_SHIFT;
		char* end = start + bytes;

		// 把大块内存切割成小块放入到span中的_freeList中管理起来
		// 1. 先切下一小块内存放入_freeList中，方便尾插
		span->_freeList = start;
		start += size;
		void* tail = span->_freeList;

		// 2. 将后面的内存切成小块尾插到_freeList中
		while (start < end)
		{
			NextObj(tail) = start;
			tail = NextObj(tail);

			start += size;
		}

		// 从page cache申请到的span要头插到当前的spanlist中，此过程对central cache中的spanlist操作，桶锁加锁保护
		spanList.GetMutex().lock();
		spanList.PushFront(span);

		return span;
	}

	// 从central cache获取一定数量的内存对象给thread cache
	size_t FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size)
	{
		// 计算要获取的内存大小映射到哪个自由链表桶
		size_t index = SizeClass::Index(size);

		_spanListBucket[index].GetMutex().lock(); // 桶锁加锁

		// 获取一个span
		Span* span = GetOneSpan(_spanListBucket[index], size);
		assert(span);
		assert(span->_freeList);

		// 在span中取batchNum个内存块，若span下挂的内存块不够batchNum个，则有多少取多少
		start = span->_freeList;
		end = start;
		size_t i = 0;
		size_t actualNum = 1; // 实际从central cache中申请到内存块的数量
		while (i < batchNum - 1 && NextObj(end) != nullptr)
		{
			end = NextObj(end);
			++i;
			++actualNum;
		}

		span->_freeList = NextObj(end);
		NextObj(end) = nullptr;
		span->_useCount += actualNum;

		_spanListBucket[index].GetMutex().unlock(); // 桶锁解锁

		return actualNum;
	}

private:
	SpanList _spanListBucket[N_FREELISTS]; // 自由链表桶

private:
	CentralCache() {}

	CentralCache(const CentralCache&) = delete;

	static CentralCache _singleInstance;
};
CentralCache CentralCache::_singleInstance;