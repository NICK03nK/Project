#pragma once

#include <cassert>
#include <mutex>

#ifdef _WIN64
typedef unsigned long long PAGE_ID;
#elif _WIN32
typedef size_t PAGE_ID;
#endif

// 管理多个连续页大块内存跨度的结构
struct Span
{
	PAGE_ID _pageId = 0;       // 页号
	size_t _nPages = 0;		   // 页的数量
	Span* _prev = nullptr;     // 指向前一个Span对象
	Span* _next = nullptr;     // 指向后一个Span
	size_t _useCount = 0;	   // 切割的小块内存，分配给thread cache的计数
	void* _freeList = nullptr; // 管理切割的小块内存的自由链表
};

// 带头双向循环链表
class SpanList
{
public:
	SpanList()
	{
		_head = new Span();
		_head->_prev = _head;
		_head->_next = _head;
	}

	Span* Begin() { return _head->_next; }

	Span* End() { return _head; }

	void PushFront(Span* newSpan)
	{
		Insert(Begin(), newSpan);
	}

	void Insert(Span* pos, Span* newSpan)
	{
		assert(pos);
		assert(newSpan);

		Span* prev = pos->_prev;
		prev->_next = newSpan;
		newSpan->_prev = prev;
		newSpan->_next = pos;
		pos->_prev = newSpan;
	}

	void Erase(Span* pos)
	{
		assert(pos);
		assert(pos != _head);

		Span* prev = pos->_prev;
		Span* next = pos->_next;

		prev->_next = next;
		next->_prev = prev;
	}

	std::mutex& GetMutex() { return _mtx; }

private:
	Span* _head;     // 头结点
	std::mutex _mtx; // 桶锁，属于当前这个自由链表桶的锁
};