#pragma once

#include <cassert>

#include "SpanList.hpp"
#include "SizeClass.hpp"

// 单例模式--饿汉模式
class CentralCache
{
public:
	// 获取central cache的单例对象
	static CentralCache* GetInstance() { return &_singleInstance; }

	// 获取一个非空的span
	Span* GetOneSpan(SpanList& spanList, size_t size)
	{
		return nullptr; // TODO
	}

	// 从central cache获取一定数量的内存对象给thread cache
	size_t FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size)
	{
		// 计算要获取的内存大小映射到哪个自由链表桶
		size_t index = SizeClass::Index(size);

		size_t actualNum = 1; // 实际从central cache中申请到内存块的数量
		{
			std::lock_guard<std::mutex> lock(_spanList[index].GetMutex()); // 桶锁，作用域内为加锁区域

			// 获取一个span
			Span* span = GetOneSpan(_spanList[index], size);
			assert(span);
			assert(span->_freeList);

			// 在span中取batchNum个内存块，若span下挂的内存块不够batchNum个，则有多少取多少
			start = span->_freeList;
			end = start;
			size_t i = 0;
			while (i < batchNum - 1 && NextObj(end) != nullptr)
			{
				end = NextObj(end);
				++i;
				++actualNum;
			}

			span->_freeList = NextObj(end);
			NextObj(end) = nullptr;
		}

		return actualNum;
	}

private:
	SpanList _spanList[N_FREELISTS]; // 自由链表桶

private:
	CentralCache() {}

	CentralCache(const CentralCache&) = delete;

	static CentralCache _singleInstance;
};
CentralCache CentralCache::_singleInstance;