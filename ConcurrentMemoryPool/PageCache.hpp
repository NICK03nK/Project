#pragma once

#include <mutex>

#include "SpanList.hpp"
#include "SizeClass.hpp"

class PageCache
{
public:
	static PageCache* GetInstance() { return &_singleInstance; }

	// 获取一个nPages页的Span对象
	Span* GetSpan(size_t nPages)
	{
		return nullptr; // TODO
	}

private:
	SpanList _spanListBucket[N_PAGES]; // 自由链表桶
	std::mutex _pageMtx;               // page cache的整体锁

private:
	PageCache() {}

	PageCache(const PageCache&) = delete;

	static PageCache _singleInstance;
};
PageCache PageCache::_singleInstance;