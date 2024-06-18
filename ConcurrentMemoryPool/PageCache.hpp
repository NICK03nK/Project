#pragma once

#include <mutex>
#include <cassert>
#include <Windows.h>

#include "SpanList.hpp"
#include "SizeClass.hpp"

// 直接去堆上按页申请空间
inline static void* SystemAlloc(size_t nPages)
{
	void* ptr = VirtualAlloc(0, nPages << 13, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (ptr == nullptr) throw std::bad_alloc();

	return ptr;
}

class PageCache
{
public:
	static PageCache* GetInstance() { return &_singleInstance; }

	// 获取一个nPages页的Span对象
	Span* GetSpan(size_t nPages)
	{
		assert(nPages > 0 && nPages < N_PAGES);

		// 先检查第nPages个SpanList中有没有span
		if (!_spanListBucket[nPages].Empty())
		{
			return _spanListBucket[nPages].PopFront();
		}

		// 执行到这说明，_spanListBucket[nPages]中没有span，则检查后面的SpanList中有没有span，如果有则将大的span切割
		// 将这个n页的span切割成一个nPages的span和一个n-nPages的span
		// nPages的span返回给central cache，n-nPages的span挂到_spanListBucket[n - nPages]中
		for (int i = nPages + 1; i < N_PAGES; ++i)
		{
			if (!_spanListBucket[i].Empty())
			{
				Span* bigSpan = _spanListBucket[i].PopFront(); // 将_spanListBucket[i]中的大块的span拿出来
				Span* nPagesSpan = new Span();

				// 在大的span的头部切割一个nPages页的span
				nPagesSpan->_pageId = bigSpan->_pageId;
				nPagesSpan->_nPages = nPages;

				// 更新完后的bigSpan就是注释中的n-nPages的span，要挂到_spanListBucket[n - nPages]中
				bigSpan->_pageId += nPages;
				bigSpan->_nPages -= nPages;

				// 将bigSpan挂到_spanListBucket[n - nPages]中
				_spanListBucket[bigSpan->_nPages].PushFront(bigSpan);

				return nPagesSpan;
			}
		}

		// 代码运行到这说明，_spanListBucket[nPages]之后一直到_spanListBucket[128]都没有大块的span了
		// 此时需要向操作系统申请一个128页的span
		Span* newSpan = new Span();
		void* ptr = SystemAlloc(N_PAGES - 1);
		newSpan->_pageId = (PAGE_ID)ptr >> PAGE_SHIFT;
		newSpan->_nPages = N_PAGES - 1;

		// 将newSpan挂到128页的_spanListBucket[128]中
		_spanListBucket[newSpan->_nPages].PushFront(newSpan);

		// 递归调一次，将刚刚申请的128页的span切割成需要的nPages页的span
		return GetSpan(nPages);
	}

	std::mutex& GetMutex() { return _pageMtx; }

private:
	SpanList _spanListBucket[N_PAGES]; // 自由链表桶
	std::mutex _pageMtx;               // page cache的整体锁

private:
	PageCache() {}

	PageCache(const PageCache&) = delete;

	static PageCache _singleInstance;
};
PageCache PageCache::_singleInstance;