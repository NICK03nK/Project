#pragma once

#include <iostream>
#include <cassert>

#include "ThreadCache.hpp"
#include "PageCache.hpp"
#include "SizeClass.hpp"

static void* ConcurrentAlloc(size_t size)
{
	if (size > MAX_BYTES)
	{
		size_t alignSize = SizeClass::RoundUp(size);
		size_t nPages = alignSize >> PAGE_SHIFT;

		PageCache::GetInstance()->GetMutex().lock();
		Span* span = PageCache::GetInstance()->GetSpan(nPages);
		PageCache::GetInstance()->GetMutex().unlock();

		void* ptr = (void*)(span->_pageId << PAGE_SHIFT);

		return ptr;
	}
	else
	{
		// 通过TLS，每个线程可以无锁的获取属于自己的专属的ThreadCache对象
		if (pTLSThreadCache == nullptr)
		{
			// 线程局部存储为空，则new一个ThreadCache对象
			pTLSThreadCache = new ThreadCache();
		}

		std::cout << std::this_thread::get_id() << ":" << pTLSThreadCache << std::endl;

		return pTLSThreadCache->Allocate(size);
	}
}

static void ConcurrentFree(void* ptr, size_t size)
{
	if (size > MAX_BYTES)
	{
		Span* span = PageCache::GetInstance()->MapObjToSpan(ptr);

		PageCache::GetInstance()->GetMutex().lock();
		PageCache::GetInstance()->ReleaseSpanToPageCache(span);
		PageCache::GetInstance()->GetMutex().unlock();
	}
	else
	{
		assert(pTLSThreadCache);

		pTLSThreadCache->Deallocate(ptr, size);
	}
}