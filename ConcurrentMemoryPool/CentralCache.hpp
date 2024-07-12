#pragma once

#include "Common.hpp"
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
		span->_isUse = true; // 从page cache中申请到的span置为使用状态
		span->_objSize = size; // 设置span下挂的小内存块的大小
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
		NextObj(tail) = nullptr;

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

	// 将批量的小内存块归还给central cache中对应的spanList的span
	// （这里函数名以Spans命名，因为归还回来的小内存块可能会过多，要归还到多个span中）
	void ReleaseListToSpans(void* start, size_t size)
	{
		size_t index = SizeClass::Index(size);

		_spanListBucket[index].GetMutex().lock(); // 桶锁加锁

		while (start)
		{
			void* next = NextObj(start);

			// 找到start对应的span，将start指向的小内存块头插到span中freeList管理的自由链表中
			Span* span = PageCache::GetInstance()->MapObjToSpan(start);
			NextObj(start) = span->_freeList;
			span->_freeList = start;
			--span->_useCount; // 每归还一个小内存块就要对span的_useCount减减

			// 当span的_useCount等于0，说明该span切割的小内存已经全部归还回来了
			// 可以将该span归还给page cache，page cache可以尝试去做前后页合并
			if (span->_useCount == 0)
			{
				_spanListBucket[index].Erase(span);
				span->_freeList = nullptr;
				span->_prev = nullptr;
				span->_next = nullptr;

				_spanListBucket[index].GetMutex().unlock();

				PageCache::GetInstance()->GetMutex().lock();
				PageCache::GetInstance()->ReleaseSpanToPageCache(span); // 将span归还给page cache
				PageCache::GetInstance()->GetMutex().unlock();

				_spanListBucket[index].GetMutex().lock();
			}

			start = next;
		}

		_spanListBucket[index].GetMutex().unlock(); // 桶锁解锁
	}

private:
	SpanList _spanListBucket[N_FREELISTS]; // 自由链表桶

private:
	CentralCache() {}

	CentralCache(const CentralCache&) = delete;

	static CentralCache _singleInstance;
};
CentralCache CentralCache::_singleInstance;