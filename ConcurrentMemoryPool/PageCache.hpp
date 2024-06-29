#pragma once

#include <mutex>
#include <cassert>
#include <Windows.h>
#include <unordered_map>

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

	std::mutex& GetMutex() { return _pageMtx; }

	// 通过小内存块得到映射的span对象
	Span* MapObjToSpan(void* obj)
	{
		PAGE_ID id = ((PAGE_ID)obj >> PAGE_SHIFT);
		auto ret = _idSpanMap.find(id);
		if (ret != _idSpanMap.end())
		{
			return ret->second;
		}
		else
		{
			assert(false);
			return nullptr;
		}
	}

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

				// 存储bigSpan的首尾页号跟bigSpan的映射，方便page cache回收内存时进行的合并查找
				_idSpanMap[bigSpan->_pageId] = bigSpan;
				_idSpanMap[bigSpan->_pageId + bigSpan->_nPages - 1] = bigSpan;

				// 建立页号和span的映射关系，方便central cache回收小块内存时查找对应的span对象
				for (PAGE_ID i = 0; i < nPagesSpan->_nPages; ++i)
				{
					_idSpanMap[nPagesSpan->_pageId + i] = nPagesSpan;
				}

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

	// 归还空闲的span到page cache，并合并相邻的span
	void ReleaseSpanToPageCache(Span* span)
	{
		// 对span前的页尝试进行合并
		while (true)
		{
			PAGE_ID prevId = span->_pageId - 1;
			auto ret = _idSpanMap.find(prevId); // 查找span前面的页

			// 前面的页号没有申请到，无法合并页，break跳出循环
			if (ret == _idSpanMap.end()) break;

			// 前面页的span正在被使用，无法合并页，break跳出循环
			Span* prevSpan = ret->second;
			if (prevSpan->_isUse == true) break;

			// 合并出超过128页的span，没办法管理，无法合并页，break跳出循环
			if (prevSpan->_nPages + span->_nPages > N_PAGES - 1) break;

			// 开始向前合并
			span->_pageId = prevSpan->_pageId;
			span->_nPages += prevSpan->_nPages;

			_spanListBucket[prevSpan->_nPages].Erase(prevSpan); // 将prevSpan从自由链表中解下来
			delete prevSpan;
		}

		// 对span后的页尝试进行合并
		while (true)
		{
			PAGE_ID nextId = span->_pageId + span->_nPages;
			auto ret = _idSpanMap.find(nextId);
			
			// 后面的页号没有申请到，无法合并页，break跳出循环
			if (ret == _idSpanMap.end()) break;
			
			// 后面页的span正在被使用，无法合并页，break跳出循环
			Span* nextSpan = ret->second;
			if (nextSpan->_isUse == true) break;

			// 合并出超过128页的span，没办法管理，无法合并页，break跳出循环
			if (nextSpan->_nPages + span->_nPages > N_PAGES - 1) break;

			// 开始向后合并
			span->_nPages += nextSpan->_nPages;

			_spanListBucket[nextSpan->_nPages].Erase(nextSpan); // 将将nextSpan从自由链表中解下来
			delete nextSpan;
		}

		// 将合并完的span挂入_spanListBucket[span->_nPages]中
		_spanListBucket[span->_nPages].PushFront(span);
		span->_isUse = false; // 将状态置为未被使用，使得其他的span可以对该span进行合并

		// 将span的首尾页号和span的映射存入_idSpanMap中
		_idSpanMap[span->_pageId] = span;
		_idSpanMap[span->_pageId + span->_nPages - 1] = span;
	}

private:
	SpanList _spanListBucket[N_PAGES];             // 自由链表桶
	std::mutex _pageMtx;						   // page cache的整体锁
	std::unordered_map<PAGE_ID, Span*> _idSpanMap; // 页号和span对象的映射关系

private:
	PageCache() {}

	PageCache(const PageCache&) = delete;

	static PageCache _singleInstance;
};
PageCache PageCache::_singleInstance;