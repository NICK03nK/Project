#pragma once

#include <iostream>
#include <cassert>

#include "ThreadCache.hpp"

static void* ConcurrentAlloc(size_t size)
{
	// 通过TLS，每个线程可以无锁的获取属于自己的专属的ThreadCache对象
	if (pTLSThreadCache == nullptr)
	{
		// 线程局部存储为空，则new一个ThreadCache对象
		pTLSThreadCache = new ThreadCache();
	}

	return pTLSThreadCache->Allocate(size);
}

static void ConcurrentFree(void* ptr, size_t size)
{
	assert(pTLSThreadCache);

	pTLSThreadCache->Deallocate(ptr, size);
}